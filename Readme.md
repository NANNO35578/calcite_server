# calcite server

[基于Web与Android双端的智能笔记系统](https://github.com/NANNO35578/calcite.git)服务器
智能笔记管理系统后端服务

## 技术栈
- 框架：Drogon C++ Web框架
- 数据库：MariaDB
- 文件存储：MinIO
- 全文搜索：Elasticsearch
- OCR：PaddleOCR 在线识别（消耗API Token）
- 接口：RESTful API

## API 列表

具体使用方法见[相关文档](#相关文档)

| 接口                | 方法 | 说明      |
| ------------------ | ---- | -------- |
| /api/auth/register | POST | 用户注册         |
| /api/auth/login    | POST | 用户登录         |
| /api/auth/logout   | POST | 退出登录         |
| /api/user/profile  | GET  | 获取用户信息      |
|                    |      |                 |
| /api/note/create   | POST | 新建笔记         |
| /api/note/update   | POST | 更新笔记         |
| /api/note/delete   | POST | 删除笔记         |
| /api/note/list     | GET  | 获取笔记列表      |
| /api/note/detail   | GET  | 获取笔记详情      |
| /api/note/search   | GET  | 全文搜索         |
|                    |      |                 |
| /api/tag/create    | POST | 创建标签           |
| /api/tag/list      | GET  | 获取标签列表         |
| /api/tag/bind      | POST | 绑定/解除笔记标签   |
| /api/tag/update    | POST | 更新标签           |
| /api/tag/delete    | POST | 删除标签           |
| /api/folder/create | POST | 创建文件夹           |
| /api/folder/list   | GET  | 获取文件夹列表         |
| /api/folder/update | POST | 更新文件夹           |
| /api/folder/delete | POST | 删除文件夹           |
|                    |      |                 |
| /api/file/upload   | POST | 上传文件到 MinIO        |
| /api/file/list     | GET  | 获取文件列表             |
| /api/file/delete   | POST | 删除文件（MinIO+数据库） |
| /api/file/status   | GET  | 查询文件上传状态          |
| /api/file/info     | GET  | 获取单个文件详情          |
|                    |      |                 |
| /api/ocr/recognize | POST | 上传文件进行OCR识别, 生成新笔记   |
| /api/ocr/status    | GET  | 查询OCR处理状态         |


## 项目结构
```
.
├── calcite/                # 服务主目录
│   ├── CMakeLists.txt
│   ├── config.json         # 服务配置
│   ├── main.cc             # 程序入口
│   ├── controllers/        # API请求控制器
│   │   ├── AuthController
│   │   ├── FileController
│   │   ├── NoteController
│   │   ├── NoteFolderController
│   │   ├── NoteTagController
│   │   ├── OcrController
│   │   ├── TagController
│   │   └── UserController
│   ├── models/             # 数据库ORM模型
│   ├── services/           # 业务逻辑层
│   │   ├── AuthService
│   │   ├── NoteFolderService
│   │   └── OcrService
│   ├── utils/              # 工具类
│   │   ├── EsClient        # ES客户端
│   │   ├── JwtUtil         # JWT认证
│   │   ├── MinioClient     # MinIO客户端
│   │   └── PasswordUtil    # 密码工具
│   └── test/               # 测试目录
├── docs/
│   ├── schema.md           # 数据库设计
│   └── api.md              # API接口文档
├── commands.sh             # 常用命令脚本
├── LICENSE
└── Readme.md
```

## 编译命令
```sh
mkdir -p calcite/build && cd calcite/build
cmake ..
make
```

## 相关文档
- [数据库设计](./docs/schema.md)
- [REST API 设计](./docs/api.md)

## 许可证
[GPL-3.0](./LICENSE)
> Actually, you can do whatever you want. Just leave a link redirecting to here🫠

