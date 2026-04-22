# AGENTS.md — calcite-server 项目指南

> 本文件面向 AI 编程助手。如果你对这个项目一无所知，请先阅读本文件。

---

## 项目概述

**calcite-server** 是一个基于 C++ 的智能笔记管理系统后端服务，面向 Web 与 Android 双端客户端提供 RESTful API。核心功能包括：用户认证、笔记的增删改查与全文搜索、标签与文件夹管理、文件上传（MinIO）、OCR 识别、AI 标签推荐、笔记点赞/收藏/推荐等社交化功能。

- **仓库地址**：`https://github.com/NANNO35578/calcite.git`（服务端子目录为 `calcite/`）
- **许可证**：GPL-3.0

---

## 技术栈

| 层级/组件 | 技术选型 | 说明 |
|-----------|----------|------|
| Web 框架 | [Drogon](https://github.com/drogonframework/drogon) | C++17 高性能异步 Web 框架 |
| 数据库 | MariaDB (MySQL 兼容) | 关系型数据持久化，通过 Drogon ORM 访问 |
| 对象存储 | MinIO | S3 兼容的对象存储，用于附件/文件 |
| 全文检索 | Elasticsearch 8.12.2 + IK 中文分词 | 笔记全文搜索与高亮 |
| LLM 服务 | Moonshot (Kimi) API | AI 标签推荐（早期曾使用 DeepSeek，已废弃） |
| OCR | 在线 OCR API | 原尝试 PaddleOCR 本地部署失败后改为在线方案 |
| JSON | jsoncpp | 序列化与反序列化 |
| 构建系统 | CMake >= 3.5 | 主目标 `calcite`，测试目标 `calcite_test` |
| 密码/认证 | bcrypt + JWT | `PasswordUtil` + `JwtUtil` |

---

## 项目结构

```
calcite-server/
├── calcite/                    # 服务主目录（CMake 项目根）
│   ├── main.cc                 # 入口：加载 config.json 并启动 Drogon
│   ├── config.json             # Drogon 运行时配置（监听端口、DB、线程数等）
│   ├── CMakeLists.txt          # 主 CMake 配置
│   ├── .clang-format           # 代码格式化配置（基于 LLVM）
│   ├── controllers/            # HTTP API 控制器（路由入口）
│   │   ├── AuthController      # 注册 / 登录 / 登出
│   │   ├── FileController      # 文件上传 / 列表 / 删除 / 状态查询
│   │   ├── NoteController      # 笔记 CRUD、搜索、点赞、收藏、浏览、AI 标签
│   │   ├── NoteFolderController# 文件夹 CRUD
│   │   ├── NoteTagController   # 笔记标签管理
│   │   ├── OcrController       # OCR 任务提交与状态查询
│   │   ├── RecommendController # 个性化推荐
│   │   ├── TagController       # 标签管理（旧版，部分已废弃）
│   │   └── UserController      # 用户信息
│   ├── models/                 # Drogon ORM 模型（由 drogon_ctl 自动生成，勿手动修改）
│   │   ├── Note.cc / Note.h
│   │   ├── User.cc / User.h
│   │   ├── Tag.cc / Tag.h
│   │   ├── FileResource.cc / FileResource.h
│   │   ├── NoteFolder.cc / NoteFolder.h
│   │   ├── NoteTag.cc / NoteTag.h
│   │   ├── NoteLike.cc / NoteLike.h
│   │   ├── NoteCollect.cc / NoteCollect.h
│   │   ├── SearchHistory.cc / SearchHistory.h
│   │   ├── UserAction.cc / UserAction.h
│   │   ├── UserTagStat.cc / UserTagStat.h
│   │   └── model.json          # drogon_ctl create model 的数据库连接配置
│   ├── services/               # 业务逻辑层
│   │   ├── AuthService         # Token 签发与校验
│   │   ├── KimiService         # Moonshot LLM 标签推荐
│   │   ├── DsService           # DeepSeek 标签推荐（已废弃，代码被注释）
│   │   ├── NoteFolderService   # 文件夹业务逻辑（含循环引用检查）
│   │   └── OcrService          # OCR 业务逻辑
│   ├── utils/                  # 工具类/客户端
│   │   ├── EsClient            # Elasticsearch HTTP 客户端（索引/更新/删除/搜索）
│   │   ├── JwtUtil             # JWT 生成与验证
│   │   ├── MinioClient         # MinIO S3 客户端（基于 libcurl）
│   │   └── PasswordUtil        # bcrypt 密码哈希
│   ├── test/                   # 测试目录
│   │   ├── CMakeLists.txt
│   │   └── test_main.cc        # 测试入口（使用 drogon_test）
│   └── build/                  # 本地构建目录（已加入 .gitignore）
├── docs/                       # 开发文档与历史记录
│   ├── schema.md               # 数据库表结构设计（13 张核心表）
│   ├── api.md                  # REST API 接口文档
│   └── 260*.md                 # 按日期命名的开发日志 / hotfix 记录
├── commands.sh                 # 常用命令备忘（含环境安装、ES/MinIO/Docker 操作）
├── Readme.md                   # 项目简介（中文）
└── AGENTS.md                   # 本文件
```

---

## 构建与运行

### 前置依赖

系统需已安装：
- `g++` 支持 C++17
- `cmake >= 3.5`
- Drogon 框架（含 `drogon_ctl`）
- `libjsoncpp-dev`, `uuid-dev`, `openssl/libssl-dev`, `zlib1g-dev`
- `libcurl4-openssl-dev`
- MariaDB / MySQL 客户端开发库

安装 Drogon 示例（见 `commands.sh`）：
```bash
sudo apt install libjsoncpp-dev uuid-dev openssl libssl-dev zlib1g-dev
# 然后从源码编译安装 Drogon
```

### 编译命令

```bash
cd calcite/build
cmake ..
make -j$(nproc)
```

编译产出：
- `calcite/build/calcite` —— 主服务可执行文件
- `calcite/build/test/calcite_test` —— 测试可执行文件

### 运行服务

```bash
cd calcite/build
./calcite
```

服务默认监听 `0.0.0.0:8888`，配置位于 `calcite/config.json`（数据库连接、线程数、日志级别等均在此文件）。

### 运行测试

```bash
cd calcite/build/test
./calcite_test
```

> 当前测试规模较小，主要覆盖 `EsClient` 的搜索功能。测试使用了自定义 `main` 函数以安全启动/停止 Drogon 事件循环，避免段错误。

---

## 代码组织与架构约定

### 分层职责

| 目录 | 职责 | 典型类 |
|------|------|--------|
| `controllers/` | 接收 HTTP 请求、参数校验、调用 Service/Model、返回 JSON | `NoteController` |
| `services/` | 封装业务逻辑、调用外部 API（LLM/OCR） | `KimiService`, `AuthService` |
| `models/` | Drogon ORM 实体，直接对应数据库表 | `Note`, `User`, `Tag` … |
| `utils/` | 基础设施客户端、通用工具 | `EsClient`, `MinioClient`, `JwtUtil` |

### 命名空间

- 控制器：`calcite::api::v1`
- 服务层：`calcite::services`
- 工具层：`calcite::utils`
- ORM 模型：`drogon_model::calcite`（由 `drogon_ctl` 生成）

### 路由注册方式

控制器继承 `drogon::HttpController<T>`，在类内使用宏注册路由：

```cpp
class NoteController : public drogon::HttpController<NoteController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(NoteController::createNote, "/api/note/create", Post);
  METHOD_LIST_END
  // ...
};
```

### 统一响应格式

所有 API 返回统一的 JSON 结构：

```json
{
  "code": 0,
  "message": "success",
  "data": {}
}
```

控制器内部通常通过私有辅助函数生成：

```cpp
Json::Value createResponse(int code, const std::string &message, const Json::Value &data = Json::Value());
```

- `code == 0` 表示成功，非 0 表示各类业务错误。

### 认证方式

- Header：`Authorization: Bearer <token>`
- Query 参数：`?token=<token>`（部分接口兼容）
- Token 由 `JwtUtil` 签发，同时在 `user_token` 表中有持久化记录，支持多端登录。
- 控制器中通常调用 `verifyTokenAndGetUserId(req, callback)` 异步校验。

---

## 数据库与 ORM

### 模型生成

**严禁手动编写或修改 `models/` 下的 `.cc` / `.h` 文件。** 这些文件由 `drogon_ctl create model` 根据数据库表结构自动生成。相关配置在 `calcite/models/model.json`。

正确做法：
1. 修改数据库表结构（`docs/schema.md` 有完整建表语句）。
2. 在 `models/` 目录下执行 `drogon_ctl create model .`
3. 重新编译。

### 核心表（13 张）

- `user` — 用户
- `user_token` — 登录令牌
- `note` — 笔记（含软删除标记 `is_deleted`、公开标记 `is_public`、计数字段）
- `note_history` — 笔记历史版本
- `note_folder` — 文件夹（支持多级，通过 `parent_id`）
- `tag` — 标签
- `note_tag` — 笔记-标签关联
- `file_resource` — 文件元数据（状态：`processing` / `done` / `failed`）
- `note_collect` — 收藏
- `note_like` — 点赞
- `search_history` — 搜索历史
- `user_action` — 用户行为（浏览/点赞/收藏）
- `user_tag_stat` — 用户标签行为统计（用于推荐算法）

---

## 外部服务集成

### Elasticsearch

- 索引名：`notes`
- 中文分词器：`ik_max_word`
- `EsClient` 提供异步/同步两种接口：索引文档、更新、删除、搜索、聚合查热门标签。
- 数据同步策略：**先写 MariaDB，后异步同步 ES**。ES 失败不阻塞主流程。
- 搜索时先做 ES 全文检索，再根据返回的 ID 列表回查 MariaDB 补全详情。

### MinIO

- 默认 endpoint：`http://127.0.0.1:9000`
- Bucket：`notes-files`
- 文件上传采用**异步模式**：接口立即返回 `file_id` 与 `processing` 状态，后台线程完成实际上传后更新为 `done`。
- Object key 规则：`userId/yyyy/mm/dd/uuid.ext`

### Moonshot (Kimi) LLM

- 用途：根据笔记内容推荐 5 个标签。
- 配置硬编码在 `KimiService.h` 中（`API_HOST`、`API_TOKEN`、`API_MODEL` 等）。
- 旧版 `DsService`（DeepSeek）已被完全注释弃用。

---

## 代码风格

项目使用 `.clang-format` 进行格式化，核心规则：

- 基于 `LLVM` 预设
- `ColumnLimit: 0`（不限制行宽）
- 短函数、短 if、短 case、短循环**允许单行**
- Lambda 体前换行：`BeforeLambdaBody: true`
- 控制语句后不换行：`AfterControlStatement: Never`
- 对齐连续赋值/声明/宏：`AcrossComments`

**请在提交前对修改的文件执行 `clang-format`。**

---

## 测试策略

- 框架：Drogon 内置 `drogon_test`（宏如 `DROGON_TEST`, `CHECK`）。
- 目录：`calcite/test/`
- 当前测试以集成测试为主，需要本地 ES 服务正常运行。
- 测试 `main` 函数做了特殊处理：在独立线程启动 `drogon::app().run()`，主线程执行测试套件，完成后安全 `quit()` 并 `join`，避免事件循环导致的崩溃。

### 添加新测试

在 `test_main.cc` 中新增：

```cpp
DROGON_TEST(YourTestName) {
    // 测试逻辑
    CHECK(condition);
}
```

然后重新 `cmake && make`。

---

## 安全注意事项

> ✅ 以下硬编码问题**已修复**，敏感配置现通过**环境变量**或 **`config.local.json`** 注入。

### 运行时依赖的环境变量

| 环境变量 | 说明 | 影响 |
| -------- | ---- | ---- |
| `CALCITE_JWT_SECRET` | JWT 签名密钥 | 未设置时 JWT 不安全，日志会输出警告 |
| `CALCITE_KIMI_API_TOKEN` | Moonshot (Kimi) API Token | 未设置时 AI 标签推荐功能不可用 |
| `CALCITE_MINIO_ACCESS_KEY` | MinIO Access Key | 未设置时文件上传/删除会失败 |
| `CALCITE_MINIO_SECRET_KEY` | MinIO Secret Key | 同上 |

### 本地配置文件 `config.local.json`

- 位于 `calcite/config.local.json`，**已加入 `.gitignore`**，请勿提交到版本控制。
- 用于覆盖 `config.json` 中的数据库密码等 Drogon 运行时配置。
- 模板参考 `calcite/config.local.json.example`。

示例：
```json
{
    "db_clients": [
        {
            "name": "default",
            "passwd": "your_actual_db_password"
        }
    ]
}
```

### ORM 模型配置 `model.json`

- `calcite/models/model.json` 中的数据库密码已清空，执行 `drogon_ctl create model .` 前请手动填入密码，或使用脚本注入。

### 历史残留

- `DsService.h` 中的旧版 DeepSeek Token 已清空（该文件已整体注释废弃）。

---

## 常用命令速查

```bash
# 构建
cd calcite/build && cmake .. && make -j$(nproc)

# 运行服务
cd calcite/build && ./calcite

# 运行测试
cd calcite/build/test && ./calcite_test

# 重新生成 ORM 模型（在 models/ 目录执行）
cd calcite/models && drogon_ctl create model .

# 格式化代码（示例）
clang-format -i calcite/controllers/NoteController.cc
```

---

## 文档索引

- `Readme.md` — 项目简介、API 速览、目录结构
- `docs/schema.md` — 数据库设计（含完整建表 SQL）
- `docs/api.md` — REST API 详细文档（请求参数、响应示例、ES 集成说明）
- `docs/260*.md` — 开发日志与 hotfix 记录，可用于追溯历史设计决策
- `commands.sh` — 环境搭建、ES/Docker/MinIO/PaddleOCR 安装命令备忘

---

## 给 AI 助手的特别提醒

1. **不要手动修改 `models/` 下的 ORM 文件**。如果需要改表结构，改 SQL → 用 `drogon_ctl` 重新生成 → 再编译。
2. **控制器里注意异步回调的生命周期**。Drogon 大量使用 `std::function<void(...)>` 回调，捕获 `this` 时要确保对象存活；避免在 lambda 中直接操作已销毁的局部变量。
3. **保持统一响应格式**。任何新增 API 都应返回 `{"code": ..., "message": ..., "data": ...}`。
4. **ES 与 DB 的同步是松耦合的**。写数据库后调用 `EsClient` 的异步接口进行索引，不要假设 ES 操作一定成功。
5. **项目注释与文档以中文为主**，新添加的注释建议继续使用中文，以保持风格一致。
