## 数据库表结构设计（核心 8 张表）

### 建库

```sql
CREATE DATABASE IF NOT EXISTS calcite
CHARACTER SET utf8mb4;

USE calcite;
```

### 1 用户表 `user`

用途：存储用户基本信息与认证数据

```sql
CREATE TABLE user (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100),
    password_hash VARCHAR(255) NOT NULL,
    avatar VARCHAR(255),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);
```


### 2 登录令牌表 `user_token`

用途：实现 Token 鉴权，支持多端登录

```sql
CREATE TABLE user_token (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    token VARCHAR(255) NOT NULL,
    expired_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES user(id)
);
```


### 3 笔记表 `note`

支持全文搜索<br>`summary` 用于智能摘要

```sql
CREATE TABLE note (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    title VARCHAR(255),
    content LONGTEXT,
    summary TEXT,
    folder_id BIGINT,
    is_deleted TINYINT DEFAULT 0,
    updated_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FULLTEXT KEY ft_content (title, content),
    FOREIGN KEY (user_id) REFERENCES user(id)
);
```


### 4 笔记历史表 `note_history`

支持版本回溯

```sql
CREATE TABLE note_history (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    note_id BIGINT NOT NULL,
    content LONGTEXT,
    version INT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (note_id) REFERENCES note(id)
);
```


### 5 文件夹表 `note_folder`

支持多级目录

```sql
CREATE TABLE note_folder (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    name VARCHAR(100),
    parent_id BIGINT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES user(id)
);
```


### 6 标签表 `tag`

```sql
CREATE TABLE tag (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    name VARCHAR(50),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES user(id)
);
```


### 7 笔记-标签关联表 `note_tag`

支持一对多、多对多关系

```sql
CREATE TABLE note_tag (
    note_id BIGINT NOT NULL,
    tag_id BIGINT NOT NULL,
    PRIMARY KEY (note_id, tag_id),
    FOREIGN KEY (note_id) REFERENCES note(id),
    FOREIGN KEY (tag_id) REFERENCES tag(id)
);
```


### 8 附件表 `file_resource`

文件路径存储，文件本体存磁盘或对象存储

```sql
CREATE TABLE file_resource (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    note_id BIGINT,
    file_name VARCHAR(255) NOT NULL,
    file_path VARCHAR(512),  -- 本地临时路径（可选）
    file_type VARCHAR(50),   -- 文件MIME类型
    file_size BIGINT,        -- 新增：文件大小（字节）
    object_key VARCHAR(255) NOT NULL,  -- 新增：MinIO 唯一存储key
    url VARCHAR(512),       -- 新增：MinIO 访问URL
    -- 状态：processing=上传中 done=成功 failed=失败
    status ENUM('processing', 'done', 'failed') DEFAULT 'processing',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    -- 外键
    FOREIGN KEY (user_id) REFERENCES user(id),
    FOREIGN KEY (note_id) REFERENCES note(id),
    
    -- 索引（加速查询）
    INDEX idx_user_id (user_id),
    INDEX idx_note_id (note_id),
    INDEX idx_status (status)
);
```

---
