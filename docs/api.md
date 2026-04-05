# REST API 设计（前后端对接核心）

## API 设计原则

* RESTful 风格
* JSON 数据格式
* Token 鉴权
* 统一返回格式

**统一响应格式：**
```json
{
  "code": 0,        // 0 表示成功，非0表示失败
  "message": "success",
  "data": {}
}
```

**鉴权方式：**
- Header: `Authorization: Bearer {token}`
- URL 参数: `?token={token}`

### API 总览

| 接口                | 方法 | 说明      | 附加参数 |
| ------------------ | ---- | -------- | -------- |
| /api/auth/register | POST | 用户注册         |         |
| /api/auth/login    | POST | 用户登录         |         |
| /api/auth/logout   | POST | 退出登录         |         |
| /api/user/profile  | GET  | 获取用户信息      |         |
|                    |      |                 |         |
| /api/note/create   | POST | 新建笔记         |         |
| /api/note/update   | POST | 更新笔记         |         |
| /api/note/delete   | POST | 删除笔记         |         |
| /api/note/list     | GET  | 获取笔记列表      | folder_id, tag_ids |
| /api/note/detail   | GET  | 获取笔记详情      |         |
| /api/note/search   | GET  | 全文搜索         |         |
|                    |      |                 |         |
| /api/tag/create    | POST | 创建标签         |         |
| /api/tag/list      | GET  | 获取标签列表      |    note_id     |
| /api/tag/bind      | POST | 绑定/解除笔记标签 |         |
| /api/folder/create | POST | 创建文件夹       |         |
| /api/folder/list   | GET  | 获取文件夹列表    |   folder_id      |
|                    |      |                 |         |

---

## 1. 用户模块 API

| 接口               | 方法   | 说明       |
| ------------------ | ---- | -------- |
| /api/auth/register | POST | 用户注册   |
| /api/auth/login    | POST | 用户登录   |
| /api/auth/logout   | POST | 退出登录   |
| /api/user/profile  | GET  | 获取用户信息 |

### 1.1 用户注册 POST /api/auth/register

**请求示例：**
```json
{
  "username": "testuser",
  "email": "test@example.com",
  "password": "password123"
}
```

**请求参数：**
| 参数     | 类型   | 必填 | 说明           |
| -------- | ------ | ------ | -------------- |
| username | string | 是     | 用户名       |
| email    | string | 否     | 邮箱         |
| password | string | 是     | 密码         |

**响应示例：**
```json
{
  "code": 0,
  "message": "注册成功",
  "data": {
    "user_id": 1,
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
  }
}
```

### 1.2 用户登录 POST /api/auth/login

**请求示例：**
```json
{
  "username": "testuser",
  "password": "password123"
}
```

**请求参数：**
| 参数     | 类型   | 必填 | 说明     |
| -------- | ------ | ------ | -------- |
| username | string | 是     | 用户名   |
| password | string | 是     | 密码     |

**响应示例：**
```json
{
  "code": 0,
  "message": "登录成功",
  "data": {
    "user_id": 1,
    "username": "testuser",
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
  }
}
```

### 1.3 退出登录 POST /api/auth/logout

**请求方式：**
- Header: `Authorization: Bearer {token}`
- 或 Body 参数: `{"token": "your_jwt_token"}`

**请求示例：**
```json
{
  "token": "your_jwt_token"
}
```

**响应示例：**
```json
{
  "code": 0,
  "message": "退出登录成功",
  "data": {}
}
```

### 1.4 获取用户信息 GET /api/user/profile

**请求方式：**
Header: `Authorization: Bearer {token}`

**响应示例：**
```json
{
  "code": 0,
  "message": "获取用户信息成功",
  "data": {
    "user_id": 1,
    "username": "testuser",
    "email": "test@example.com",
    "created_at": "2025-01-01 12:00:00"
  }
}
```

---

## 2. 笔记管理 API（核心）

| 接口               | 方法   | 说明       |
| ---------------- | ---- | -------- |
| /api/note/create | POST | 新建笔记   |
| /api/note/update | POST | 更新笔记   |
| /api/note/delete | POST | 删除笔记   |
| /api/note/list   | GET  | 获取笔记列表 |
| /api/note/detail | GET  | 获取笔记详情 |
| /api/note/search | GET  | 全文搜索   |

**鉴权要求：** 所有笔记接口均需通过 Token 鉴权

### 2.1 创建笔记 POST /api/note/create

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "title": "我的第一篇笔记",
  "content": "这是一篇测试笔记内容...",
  "summary": "笔记摘要",
  "folder_id": 1
}
```

**请求参数：**
| 参数      | 类型   | 必填 | 说明                     |
| --------- | ------ | ------ | ------------------------ |
| title     | string | 是     | 笔记标题                 |
| content   | string | 是     | 笔记内容                 |
| summary   | string | 否     | 笔记摘要                 |
| folder_id | int64  | 否     | 所属文件夹ID，0表示未分类 |

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

### 2.2 更新笔记 POST /api/note/update

**请求方式：**
Header: `Authorization: Bearer {token}`

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

**请求参数：**
| 参数      | 类型   | 必填 | 说明                       |
| --------- | ------ | ------ | -------------------------- |
| note_id   | int64  | 是     | 笔记ID                     |
| title     | string | 否     | 更新标题                   |
| content   | string | 否     | 更新内容                   |
| summary   | string | 否     | 更新摘要                   |
| folder_id | int64  | 否     | 更新文件夹ID，>=0表示更新 |

**响应示例：**
```json
{
  "code": 0,
  "message": "更新笔记成功",
  "data": {}
}
```

### 2.3 删除笔记 POST /api/note/delete

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "note_id": 1
}
```

**请求参数：**
| 参数    | 类型  | 必填 | 说明     |
| ------- | ----- | ------ | -------- |
| note_id | int64 | 是     | 笔记ID   |

**响应示例：**
```json
{
  "code": 0,
  "message": "删除笔记成功",
  "data": {}
}
```

> 注：删除为软删除，仅标记 `is_deleted` 字段，实际数据仍保留

### 2.4 获取笔记列表 GET /api/note/list

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求参数：**
| 参数       | 类型   | 必填 | 说明                                               |
| ---------- | ------ | ------ | -------------------------------------------------- |
| folder_id | int64  | 否     | 文件夹ID，0表示所有笔记，不传表示未分类笔记       |
| tag_ids   | string | 否     | 标签ID数组，用逗号分隔，如 "1,2,3"，空表示不过滤 |

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

### 2.5 获取笔记详情 GET /api/note/detail

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求参数：**
| 参数    | 类型   | 必填 | 说明     |
| ------- | ------ | ------ | -------- |
| note_id | string | 是     | 笔记ID   |

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

### 2.6 全文搜索笔记 GET /api/note/search

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求参数：**
| 参数    | 类型   | 必填 | 说明                    |
| ------- | ------ | ------ | ---------------------- |
| keyword | string | 是     | 搜索关键词              |
| from    | int    | 否     | 分页起始位置，默认0     |
| size    | int    | 否     | 每页数量，默认20，最大100 |

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
      "updated_at": "2025-01-01 12:00:00",
      "highlight_title": "我的第一篇<mark>笔记</mark>",
      "highlight_content": "这是一篇关于<mark>笔记</mark>的测试内容...",
      "score": 1.25
    }
  ]
}
```

**说明：**
- 搜索基于 Elasticsearch 实现，支持全文检索
- 搜索匹配字段（按权重排序）：title(3x)、tags(2x)、summary(2x)、content(1x)
- 支持模糊匹配（fuzziness: AUTO）
- 返回结果按相关度分数降序排列，分数相同按更新时间降序
- `highlight_title`: 标题中的匹配高亮片段（使用 `<mark>` 标签）
- `highlight_content`: 内容中的匹配高亮片段
- `score`: 匹配相关度分数
- 响应不包含完整 `content` 字段，需要详情请调用 `/api/note/detail`

---

## Elasticsearch 集成说明

### 数据同步机制

系统采用 **"先写数据库，后异步同步ES"** 的策略：

| 操作   | MariaDB                     | Elasticsearch              |
| ------ | --------------------------- | -------------------------- |
| 创建   | 先插入记录，获取主键ID       | 异步索引文档，ID与数据库一致 |
| 更新   | 先更新记录                   | 异步部分更新文档            |
| 删除   | 标记 `is_deleted=1`（软删除）| 异步删除文档                |
| 搜索   | 不参与查询                   | 执行全文检索，返回匹配ID列表 |

### ES索引结构

索引名称：`notes`

文档字段映射：
| 字段        | 类型       | 说明                    |
| ----------- | ---------- | ---------------------- |
| user_id     | long       | 用户ID，用于权限过滤     |
| title       | text       | 标题，带keyword子字段    |
| content     | text       | 内容                    |
| summary     | text       | 摘要                    |
| tags        | keyword    | 标签数组                |
| updated_at  | date       | 更新时间                |

### 搜索流程

1. 客户端发起搜索请求（带关键词）
2. 服务端在ES中执行多字段匹配查询（multi_match）
3. 应用用户ID过滤，确保只能搜索自己的笔记
4. 获取匹配的文档ID列表及高亮片段
5. 根据ID列表从MariaDB批量查询完整记录
6. 按ES返回顺序排序，合并高亮信息
7. 返回给客户端

### 容错处理

- ES连接失败不影响数据库写入操作
- ES操作失败会记录错误日志，不会阻塞主流程
- 搜索时如果ES不可用，返回空结果

---

## 3. 标签与分类 API

| 接口             | 方法   | 说明               |
| ---------------- | ---- | ------------------ |
| /api/tag/create    | POST | 创建标签           |
| /api/tag/list      | GET  | 获取标签列表         |
| /api/tag/bind      | POST | 绑定/解除笔记标签   |
| /api/tag/update    | POST | 更新标签           |
| /api/tag/delete    | POST | 删除标签           |
| /api/folder/create | POST | 创建文件夹           |
| /api/folder/list   | GET  | 获取文件夹列表         |
| /api/folder/update | POST | 更新文件夹           |
| /api/folder/delete | POST | 删除文件夹           |


**鉴权要求：** 所有接口均需通过 Token 鉴权

### 3.1 创建标签 POST /api/tag/create

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "name": "工作"
}
```

**请求参数：**
| 参数 | 类型   | 必填 | 说明     |
| ---- | ------ | ------ | -------- |
| name | string | 是     | 标签名称 |

**响应示例：**
```json
{
  "code": 0,
  "message": "创建标签成功",
  "data": {
    "tag_id": 1
  }
}
```

### 3.2 获取标签列表 GET /api/tag/list

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求参数：**
| 参数     | 类型   | 必填 | 说明                                      |
| -------- | ------ | ------ | ----------------------------------------- |
| note_id  | int64  | 否     | 笔记ID，不传则返回用户所有标签，传则返回该笔记关联的标签 |

**响应示例：**
```json
{
  "code": 0,
  "message": "获取标签列表成功",
  "data": [
    {
      "id": 1,
      "name": "工作",
      "created_at": "2025-01-01 12:00:00"
    },
    {
      "id": 2,
      "name": "学习",
      "created_at": "2025-01-02 14:30:00"
    }
  ]
}
```

### 3.3 绑定笔记标签 POST /api/tag/bind

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "note_id": 1,
  "tag_ids": [1, 2, 3]
}
```

**请求参数：**
| 参数    | 类型         | 必填 | 说明                                     |
| ------- | ------------ | ------ | ---------------------------------------- |
| note_id | int64        | 是     | 笔记ID                                   |
| tag_ids | array&lt;int&gt; | 否     | 标签ID数组，空数组 [] 表示清除所有标签 |

**响应示例：**
```json
{
  "code": 0,
  "message": "绑定标签成功",
  "data": {}
}
```

> 注：该接口会先清除该笔记的所有标签，然后绑定新的标签。如需清除所有标签，传入 `tag_ids: []` 即可。

### 3.4 更新标签 POST /api/tag/update

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "tag_id": 1,
  "name": "新标签名"
}
```

**请求参数：**
| 参数    | 类型   | 必填 | 说明       |
| ------- | ------ | ------ | ---------- |
| tag_id  | int64  | 是     | 待更新的标签ID |
| name    | string | 是     | 新的标签名称   |

**响应示例：**
```json
{
  "code": 0,
  "message": "更新标签成功",
  "data": {}
}
```

### 3.5 删除标签 POST /api/tag/delete

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "tag_id": 1
}
```

**请求参数：**
| 参数    | 类型  | 必填 | 说明      |
| ------- | ----- | ------ | --------- |
| tag_id  | int64 | 是     | 待删除的标签ID |

**响应示例：**
```json
{
  "code": 0,
  "message": "删除标签成功",
  "data": {}
}
```

> 注：删除标签时会同时清除该标签与所有笔记的关联关系

### 3.6 创建文件夹 POST /api/folder/create

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "name": "技术文档",
  "parent_id": 0
}
```

**请求参数：**
| 参数       | 类型  | 必填 | 说明                     |
| ---------- | ----- | ------ | ------------------------ |
| name       | string | 是     | 文件夹名称               |
| parent_id | int64  | 否     | 父文件夹ID，0表示根文件夹 |

**响应示例：**
```json
{
  "code": 0,
  "message": "创建文件夹成功",
  "data": {
    "folder_id": 1
  }
}
```

### 3.7 更新文件夹 POST /api/folder/update

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "folder_id": 1,
  "name": "新文件夹名",
  "parent_id": 2
}
```

**请求参数：**
| 参数       | 类型   | 必填 | 说明                                 |
| ---------- | ------ | ------ | ------------------------------------ |
| folder_id  | int64  | 是     | 待更新的文件夹ID                        |
| name       | string | 否     | 新的文件夹名称（不更新则不传或为空）       |
| parent_id  | int64  | 否     | 新的父文件夹ID，0表示根文件夹（不更新则不传） |

**响应示例：**
```json
{
  "code": 0,
  "message": "更新文件夹成功",
  "data": {}
}
```

> 注：更新 parent_id 时会防止循环引用（不能将文件夹设为自己或自己的子文件夹的子文件夹）

### 3.8 删除文件夹 POST /api/folder/delete

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求示例：**
```json
{
  "folder_id": 1
}
```

**请求参数：**
| 参数      | 类型  | 必填 | 说明       |
| --------- | ----- | ------ | ---------- |
| folder_id | int64 | 是     | 待删除的文件夹ID |

**响应示例：**
```json
{
  "code": 0,
  "message": "删除文件夹成功",
  "data": {}
}
```

> 注：删除文件夹时会递归删除所有子文件夹，并将这些文件夹下的笔记标记为软删除（is_deleted = 1）

### 3.9 获取文件夹列表 GET /api/folder/list

**请求方式：**
Header: `Authorization: Bearer {token}`

**请求参数：**
| 参数       | 类型   | 必填 | 说明                                   |
| ---------- | ------ | ------ | -------------------------------------- |
| folder_id | int64  | 否     | 父文件夹ID，0表示获取根级文件夹（不传默认为0） |

**响应示例：**
```json
{
  "code": 0,
  "message": "获取文件夹列表成功",
  "data": [
    {
      "id": 1,
      "name": "技术文档",
      "parent_id": 0,
      "created_at": "2025-01-01 12:00:00"
    },
    {
      "id": 2,
      "name": "前端",
      "parent_id": 1,
      "created_at": "2025-01-02 10:00:00"
    }
  ]
}
```

> 注：该接口仅返回指定父文件夹的直接子文件夹，不会递归返回所有子文件夹

---


## 4. 文件与 OCR API

| 接口                | 方法   | 说明    |
| ------------------ | ---- | ----- |
| /api/file/upload   | POST | 上传附件  |
| /api/file/list     | GET  | 获取附件  |
| /api/ocr/recognize | POST | 图片转文本 |


## 5. 同步 API（双端重点）

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


## 错误码说明

| code | message             | 说明               |
| ----- | ------------------- | ------------------ |
| 0     | success            | 请求成功           |
| 1     | Token无效或已过期    | 鉴权失败           |
| 1     | 请求参数错误         | 参数格式或缺失       |
| 1     | 标题和内容不能为空     | 创建/更新笔记参数错误 |
| 1     | 笔记ID无效         | 笔记ID不存在       |
| 1     | 无权访问该笔记      | 用户无权操作该笔记   |
| 1     | 标签名称不能为空     | 创建标签参数错误     |
| 1     | 部分标签不存在或无权访问 | 绑定标签时验证失败 |
| 1     | 文件夹名称不能为空     | 创建文件夹参数错误   |

> 注意：所有非0的 code 均为失败情况，具体错误信息通过 `message` 字段返回
