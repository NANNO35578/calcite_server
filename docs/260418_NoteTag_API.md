# 笔记标签与热门标签 API 实现说明

> 文档日期：2026-04-18  
> 涉及文件：
> - `calcite/controllers/NoteController.h`
> - `calcite/controllers/NoteController.cc`
> - `calcite/controllers/TagController.h`
> - `calcite/controllers/TagController.cc`

---

## 一、API 清单

| 接口路径                    | 请求方法 | 功能说明         | 实现位置           |
| --------------------------- | -------- | ---------------- | ------------------ |
| /api/notes/{id}/tags        | GET      | 获取指定笔记的标签 | NoteController     |
| /api/notes/{id}/tags/ai     | POST     | AI 生成并保存笔记标签 | NoteController     |
| /api/tags/hot               | GET      | 获取热门标签       | TagController      |

---

## 二、GET /api/notes/{id}/tags

### 2.1 实现逻辑
1. 从路径参数 `id` 中解析笔记 ID。
2. 通过 `verifyTokenAndGetUserId` 校验 Token 并获取当前用户 ID。
3. 使用 `drogon::orm::Mapper<Note>` 查询笔记详情，校验笔记是否存在及访问权限（笔记属于当前用户或笔记为公开状态）。
4. 使用 `drogon::orm::Mapper<NoteTag>` 根据 `note_id` 查询关联记录，提取 `tag_id` 列表。
5. 使用 `drogon::orm::Mapper<Tag>` 的 `In` 查询批量获取标签名称。
6. 封装标准 JSON 返回客户端。

### 2.2 依赖类调用
- `services::AuthService::verifyToken` — Token 鉴权
- `drogon_model::calcite::Note` — 笔记存在性与权限校验
- `drogon_model::calcite::NoteTag` — 查询笔记-标签关联
- `drogon_model::calcite::Tag` — 查询标签详情

### 2.3 请求/返回示例

**请求：**
```bash
curl -X GET http://localhost:8888/api/notes/1/tags \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIs..."
```

**成功响应：**
```json
{
  "code": 0,
  "message": "获取标签列表成功",
  "data": [
    { "id": 1, "name": "Java" },
    { "id": 2, "name": "SpringBoot" }
  ]
}
```

**异常响应：**
```json
{
  "code": 1,
  "message": "无权访问该笔记",
  "data": {}
}
```

---

## 三、POST /api/notes/{id}/tags/ai

### 3.1 实现逻辑
1. 从路径参数 `id` 中解析笔记 ID。
2. 校验 Token 并获取当前用户 ID。
3. 查询笔记详情，校验所有权（仅笔记所有者可操作）。
4. 提取笔记 `content`，调用 `services::DsService::recommendTags` 进行异步 AI 标签推荐。
5. 在回调中接收 `TagRecommendationResult`，获取推荐标签名称列表。
6. 使用 `drogon::orm::Mapper<Tag>` 根据名称批量查询标签 ID。
7. 使用 `drogon::orm::Mapper<NoteTag>` 先 `deleteBy` 清除该笔记旧标签关联，再逐条 `insert` 新的关联记录。
8. 调用 `utils::EsClient::updateDocument` 将新标签列表异步更新到 Elasticsearch。
9. 返回生成成功的标签列表 JSON。

### 3.2 依赖类调用
- `services::AuthService::verifyToken` — Token 鉴权
- `drogon_model::calcite::Note` — 笔记查询与权限校验
- `services::DsService::recommendTags` — AI 标签推荐（DeepSeek API）
- `drogon_model::calcite::Tag` — 根据名称查询标签 ID
- `drogon_model::calcite::NoteTag` — 写入/删除笔记-标签关联
- `utils::EsClient::updateDocument` — 同步标签到 Elasticsearch

### 3.3 请求/返回示例

**请求：**
```bash
curl -X POST http://localhost:8888/api/notes/1/tags/ai \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIs..."
```

**成功响应：**
```json
{
  "code": 0,
  "message": "AI标签生成并保存成功",
  "data": [
    { "name": "Java" },
    { "name": "SpringBoot" },
    { "name": "微服务" }
  ]
}
```

**异常响应：**
```json
{
  "code": 1,
  "message": "AI标签生成失败: Empty note content",
  "data": {}
}
```

---

## 四、GET /api/tags/hot

### 4.1 实现逻辑
1. 校验 Token 并获取当前用户 ID。
2. 构造 Elasticsearch 聚合查询请求（`POST /notes/_search`），查询近 7 天内 `is_public=true` 的公开笔记，按 `tags` 字段聚合取 Top 10。
3. 使用 `drogon::HttpClient` 异步发送请求到 ES。
4. 解析响应中的 `aggregations.hot_tags.buckets`，提取 `key`（标签名）和 `doc_count`（出现次数）。
5. 封装为标准 JSON 返回客户端。

### 4.2 依赖类调用
- `services::AuthService::verifyToken` — Token 鉴权
- `drogon::HttpClient` — 直接访问 Elasticsearch（`http://localhost:9200`）

### 4.3 请求/返回示例

**请求：**
```bash
curl -X GET http://localhost:8888/api/tags/hot \
  -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIs..."
```

**成功响应：**
```json
{
  "code": 0,
  "message": "获取热门标签成功",
  "data": [
    { "tag": "Java", "count": 42 },
    { "tag": "SpringBoot", "count": 35 }
  ]
}
```

**异常响应：**
```json
{
  "code": 1,
  "message": "ES查询失败",
  "data": {}
}
```

---

## 五、ES 查询语句（/api/tags/hot 直接使用）

```json
POST /notes/_search
{
  "size": 0,
  "query": {
    "bool": {
      "filter": [
        { "term": { "is_public": true }},
        {
          "range": {
            "created_at": {
              "gte": "now-7d/d"
            }
          }
        }
      ]
    }
  },
  "aggs": {
    "hot_tags": {
      "terms": {
        "field": "tags",
        "size": 10
      }
    }
  }
}
```
