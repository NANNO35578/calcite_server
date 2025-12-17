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


## 2. 用户模块 API

| 接口                 | 方法   | 说明     |
| ------------------ | ---- | ------ |
| /api/auth/register | POST | 用户注册   |
| /api/auth/login    | POST | 用户登录   |
| /api/auth/logout   | POST | 退出登录   |
| /api/user/profile  | GET  | 获取用户信息 |


## 3. 笔记管理 API（核心）

| 接口               | 方法   | 说明     |
| ---------------- | ---- | ------ |
| /api/note/create | POST | 新建笔记   |
| /api/note/update | POST | 更新笔记   |
| /api/note/delete | POST | 删除笔记   |
| /api/note/list   | GET  | 获取笔记列表 |
| /api/note/detail | GET  | 获取笔记详情 |
| /api/note/search | GET  | 全文搜索   |


## 4. 标签与分类 API

| 接口                 | 方法   |
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
