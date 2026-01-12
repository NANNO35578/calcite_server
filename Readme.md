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


---

# [数据库设计](./_Design_database.md)

系统采用 **MySQL 8.0** 作为核心数据存储数据库，采用关系型数据模型，对用户信息、笔记内容、标签、附件以及同步记录进行统一管理。
数据库设计遵循 **第三范式（3NF）**，减少数据冗余，提高数据一致性和可维护性。

---

# [REST API 设计（前后端对接核心）](./_Design_REST_API.md)


