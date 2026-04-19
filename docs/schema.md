## 数据库表结构设计（核心 13 张表）

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
    is_public TINYINT DEFAULT 0,
    view_count INT DEFAULT 0,
    like_count INT DEFAULT 0,
    collect_count INT DEFAULT 0,
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
    name VARCHAR(50) NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_tag_name (name)
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
    user_id BIGINT,
    note_id BIGINT,
    file_name VARCHAR(255),
    file_path VARCHAR(255),
    file_type VARCHAR(50),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    file_size BIGINT,
    object_key VARCHAR(255),
    url VARCHAR(512),
    status ENUM('processing', 'done', 'failed') DEFAULT 'processing',
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES user(id),
    FOREIGN KEY (note_id) REFERENCES note(id)
);
```

### 9 笔记收藏表 `note_collect`

```sql
CREATE TABLE note_collect (
    user_id BIGINT NOT NULL,
    note_id BIGINT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, note_id)
);
```

### 10 笔记点赞表 `note_like`

```sql
CREATE TABLE note_like (
    user_id BIGINT NOT NULL,
    note_id BIGINT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, note_id)
);
```

### 11 搜索历史表 `search_history`

```sql
CREATE TABLE search_history (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    query VARCHAR(255),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    KEY idx_user_time (user_id, created_at)
);
```

### 12 用户行为表 `user_action`

```sql
CREATE TABLE user_action (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    note_id BIGINT NOT NULL,
    action_type TINYINT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    KEY idx_user (user_id),
    KEY idx_note (note_id),
    KEY idx_action (action_type),
    FOREIGN KEY (user_id) REFERENCES user(id),
    FOREIGN KEY (note_id) REFERENCES note(id)
);
```

### 13 用户标签统计表 `user_tag_stat`

```sql
CREATE TABLE user_tag_stat (
    user_id BIGINT NOT NULL,
    tag_id BIGINT NOT NULL,
    view_count INT DEFAULT 0,
    like_count INT DEFAULT 0,
    collect_count INT DEFAULT 0,
    last_action_time DATETIME,
    PRIMARY KEY (user_id, tag_id),
    KEY idx_user_score (user_id)
);
```

---
