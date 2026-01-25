# REST API 设计（前后端对接核心）

## 1. API 设计原则

* RESTful 风格
* JSON 数据格式
* Token 鉴权
* 统一返回格式

```json
{
  "code": 0,
  "message": "success",
  "data": {}
}
```

----

## 2. 用户模块 API

| 接口                 | 方法   | 说明     |
| ------------------ | ---- | ------ |
| /api/auth/register | POST | 用户注册   |
| /api/auth/login    | POST | 用户登录   |
| /api/auth/logout   | POST | 退出登录   |
| /api/user/profile  | GET  | 获取用户信息 |

参数示例：

#### 注册接口
```json
POST /api/auth/register
{
  "username": "testuser",
  "email": "test@example.com",  // 可选
  "password": "password123"
}
```

#### 登录接口
```json
POST /api/auth/login
{
  "username": "testuser",
  "password": "password123"
}
```

#### 退出登录接口
```json
POST /api/auth/logout
{
  "token": "your_jwt_token"
}
```
或者通过 Header: `Authorization: Bearer your_jwt_token`

#### 获取用户信息接口
```
GET /api/user/profile
Header: Authorization: Bearer your_jwt_token
```

所有接口返回统一格式：
```json
{
  "code": 0,        // 0 表示成功，非0表示失败
  "message": "success",
  "data": {}
}
```

---

## 3. 笔记管理 API（核心）

| 接口               | 方法   | 说明     |
| ---------------- | ---- | ------ |
| /api/note/create | POST | 新建笔记   |
| /api/note/update | POST | 更新笔记   |
| /api/note/delete | POST | 删除笔记   |
| /api/note/list   | GET  | 获取笔记列表 |
| /api/note/detail | GET  | 获取笔记详情 |
| /api/note/search | GET  | 全文搜索   |

### 3.1 创建笔记 POST /api/note/create

**请求示例：**
```json
{
  "title": "我的第一篇笔记",
  "content": "这是一篇测试笔记内容...",
  "summary": "笔记摘要",
  "folder_id": 1
}
```

**响应示例：**
```json
{
  "code": 0,
  "message": "创建笔记成功",
  "data": {
    "note_id": 1
  }
}
```

### 3.2 更新笔记 POST /api/note/update

**请求示例：**
```json
{
  "note_id": 1,
  "title": "更新后的标题",
  "content": "更新后的内容",
  "summary": "更新后的摘要",
  "folder_id": 2
}
```

**响应示例：**
```json
{
  "code": 0,
  "message": "更新笔记成功",
  "data": {}
}
```

### 3.3 删除笔记 POST /api/note/delete

**请求示例：**
```json
{
  "note_id": 1
}
```

**响应示例：**
```json
{
  "code": 0,
  "message": "删除笔记成功",
  "data": {}
}
```

### 3.4 获取笔记列表 GET /api/note/list

**请求参数：**
- `folder_id` (可选): 文件夹ID，0 表示所有笔记，-1 表示未分类笔记

**响应示例：**
```json
{
  "code": 0,
  "message": "获取笔记列表成功",
  "data": [
    {
      "id": 1,
      "title": "我的第一篇笔记",
      "summary": "笔记摘要",
      "folder_id": 1,
      "created_at": "2025-01-01 12:00:00",
      "updated_at": "2025-01-01 12:00:00"
    }
  ]
}
```
> 注：列表响应不包含 `content` 字段以减少网络流量，完整内容需调用 `/api/note/detail` 获取

### 3.5 获取笔记详情 GET /api/note/detail

**请求参数：**
- `note_id` (必填): 笔记ID

**响应示例：**
```json
{
  "code": 0,
  "message": "获取笔记详情成功",
  "data": {
    "id": 1,
    "title": "我的第一篇笔记",
    "content": "笔记内容...",
    "summary": "笔记摘要",
    "folder_id": 1,
    "created_at": "2025-01-01 12:00:00",
    "updated_at": "2025-01-01 12:00:00"
  }
}
```

### 3.6 全文搜索笔记 GET /api/note/search

**请求参数：**
- `keyword` (必填): 搜索关键词

**响应示例：**
```json
{
  "code": 0,
  "message": "搜索成功",
  "data": [
    {
      "id": 1,
      "title": "我的第一篇笔记",
      "summary": "笔记摘要",
      "folder_id": 1,
      "created_at": "2025-01-01 12:00:00",
      "updated_at": "2025-01-01 12:00:00"
    }
  ]
}
```
> 注：搜索响应不包含 `content` 字段以减少网络流量


## 4. 标签与分类 API

| 接口               | 方法   |
| ------------------ | ---- |
| /api/tag/create    | POST |
| /api/tag/list      | GET  |
| /api/note/tag/bind | POST |
| /api/folder/create | POST |
| /api/folder/list   | GET  |


## 5. 文件与 OCR API

| 接口                 | 方法   | 说明    |
| ------------------ | ---- | ----- |
| /api/file/upload   | POST | 上传附件  |
| /api/file/list     | GET  | 获取附件  |
| /api/ocr/recognize | POST | 图片转文本 |


## 6. 同步 API（双端重点）

| 接口             | 方法   | 说明     |
| -------------- | ---- | ------ |
| /api/sync/pull | GET  | 拉取最新数据 |
| /api/sync/push | POST | 上传变更   |

参数示例：

```json
{
  "last_sync_time": "2025-01-01 12:00:00"
}
```

---
