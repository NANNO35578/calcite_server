# calcite server

[基于Web与Android双端的智能笔记系统](https://github.com/NANNO35578/calcite.git)服务器

后端工程结构如下：

```text
sources/
├── CMakeLists.txt
├── main.cc
├── config.json
├── controllers/
│   ├── AuthController.cc
│   ├── NoteController.cc
│   └── FileController.cc
├── models/
│   └── User.h / Note.h（ORM生成）
├── services/
│   ├── AuthService.cc
│   └── NoteService.cc
├── utils/
│   ├── JwtUtil.cc
│   └── PasswordUtil.cc
└── build/
```

编译命令:
```sh
mkdir -p calcite/build && cd calcite/build
cmake ..
make
```


---

# [数据库设计](./docs/schema.md)

系统采用 **MySQL** 作为核心数据存储数据库，采用关系型数据模型，对用户信息、笔记内容、标签、附件以及同步记录进行统一管理。
数据库设计遵循 **第三范式（3NF）**，减少数据冗余，提高数据一致性和可维护性。

- 以下表使用drogon生成了orm形式的模型类: `"user","note","note_folder","tag","note_tag"`

---

# [REST API 设计（前后端对接核心）](./docs/api.md)


----

# work path

## 260125


用户模块 API 已修改为使用 Drogon ORM 模型。主要变更如下:

### AuthService.h (calcite/services/AuthService.h):

使用 `drogon::orm::Mapper<drogon_model::calcite::User>` 替代 `DbClientPtr`
移除了 `saveToken` 私有方法（JWT 模式不需要落库）

### AuthService.cc (calcite/services/AuthService.cc):

`registerUser`: 使用 `userMapper_.findBy()` 检查用户名/邮箱，使用 `userMapper_.insert()` 创建用户
`loginUser`: 使用 `userMapper_.findBy()` 查询用户，移除了 user_token 插入操作
`logoutUser`: JWT 模式下仅需客户端删除 token，服务器直接返回成功
`verifyToken`: 直接验证 JWT，无需数据库查询

### UserController.h (calcite/controllers/UserController.h):

添加了 `drogon/orm/Mapper.h` 和 `models/User.h` 引用

### UserController.cc (calcite/controllers/UserController.cc):

使用 `userMapper.findByPrimaryKey()` 按 `userId` 查询用户详情
使用模型类的 `getter` 方法（`getValueOfId()`, `getEmail()` 等）获取字段值
IDE 显示的 include 错误是配置问题，不影响实际编译。


## 260125PM

- 完成笔记管理模块API.

## 260201

- 完成基本Tag&Folder API

## 260203

- 优化Tag&Folder API

## todo: CRUD about Tag&Folder

---

# 请求&响应 示例

> See @ [API设计文档](./docs/api.md)中

