# searchNotes 重构操作记录

> 操作日期：2026-04-18  
> 执行人：Kimi Code CLI  
> 涉及范围：`calcite/controllers/NoteController.cc`、`docs/api.md`

---

## 一、操作背景与目标

### 1.1 问题发现

在审查 `NoteController::searchNotes` 实现时，发现 ES 搜索回调中存在**完全冗余的数据库查询**：

- ES 返回的结果中，`_source` 已包含 `title`、`summary`、`created_at`、`updated_at` 等全部所需字段
- 回调中又通过 `execSqlAsync` 向 MariaDB 发起批量查询：`SELECT id, title, summary, folder_id, created_at, updated_at FROM note WHERE id IN (...)`
- 该查询增加了额外的网络 RTT、数据库连接开销和序列化成本，且在高并发搜索场景下会成为性能瓶颈

### 1.2 优化目标

1. **彻底删除** `searchNotes` 中的冗余数据库查询代码
2. **直接使用** `EsSearchResult` 构造 HTTP 响应
3. **移除返回值**中的 `folder_id`（该字段无搜索业务用途）
4. **同步更新**接口文档 `api.md`

---

## 二、修改文件清单

| 文件路径 | 修改类型 | 说明 |
|----------|----------|------|
| `calcite/controllers/NoteController.cc` | 代码重构 | 删除数据库查询，简化回调逻辑 |
| `docs/api.md` | 文档更新 | 移除响应示例中的 `folder_id` 字段 |

**未修改的文件（严格遵循约束）**：
- `calcite/utils/EsClient.h`
- `calcite/utils/EsClient.cc`
- `calcite/services/*`
- `calcite/models/*`

---

## 三、代码修改详情

### 3.1 修改前：`searchNotes` 冗余逻辑

```cpp
esClient_.search(userId, isPublic, keyword, 
  [this, callback, userId, keyword](const std::vector<utils::EsSearchResult>& esResults) {
    if (esResults.empty()) {
      // 返回空数组...
      return;
    }

    // 提取ID列表
    std::vector<int64_t> noteIds;
    for (const auto& result : esResults) {
      noteIds.push_back(result.noteId);
    }

    // 构建ID列表SQL
    std::string idListStr;
    for (size_t i = 0; i < noteIds.size(); ++i) {
      if (i > 0) idListStr += ",";
      idListStr += std::to_string(noteIds[i]);
    }

    // 从MariaDB批量查询笔记详情 ← 完全冗余
    auto dbClient = drogon::app().getDbClient("default");
    std::string sql = 
      "SELECT id, title, summary, folder_id, created_at, updated_at "
      "FROM note WHERE id IN (" + idListStr + ") AND user_id = ? AND is_deleted = 0";

    dbClient->execSqlAsync(
      sql,
      [this, callback, esResults](const drogon::orm::Result &result) mutable {
        std::unordered_map<int64_t, Json::Value> noteMap;
        for (const auto &row : result) {
          // 构建 noteMap...
        }

        // 按ES返回顺序组装结果，合并高亮信息
        Json::Value data(Json::arrayValue);
        for (const auto& esResult : esResults) {
          auto it = noteMap.find(esResult.noteId);
          if (it != noteMap.end()) {
            Json::Value noteJson = it->second;
            // 合并高亮...
            data.append(noteJson);
          }
        }
        // 返回响应...
      },
      // 错误回调...
      userId
    );
  },
  from, size
);
```

### 3.2 修改后：`searchNotes` 精简逻辑

```cpp
esClient_.search(userId, isPublic, keyword, 
  [this, callback](const std::vector<utils::EsSearchResult>& esResults) {
    Json::Value data(Json::arrayValue);
    for (const auto& esResult : esResults) {
      Json::Value noteJson;
      noteJson["id"] = static_cast<Json::Int64>(esResult.noteId);
      noteJson["title"] = esResult.title;
      noteJson["summary"] = esResult.summary;
      noteJson["created_at"] = esResult.createdAt;
      noteJson["updated_at"] = esResult.updatedAt;
      
      if (!esResult.highlightTitle.empty()) {
        noteJson["highlight_title"] = esResult.highlightTitle;
      }
      if (!esResult.highlightContent.empty()) {
        noteJson["highlight_content"] = esResult.highlightContent;
      }
      noteJson["score"] = esResult.score;
      
      data.append(noteJson);
    }

    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "搜索成功", data));
    callback(resp);
  },
  from, size
);
```

### 3.3 关键变更点

| 变更项 | 变更说明 |
|--------|----------|
| **删除数据库查询** | 移除了 `execSqlAsync`、SQL 拼接、`noteMap` 构建等全部数据库相关代码 |
| **删除 ID 列表提取** | 不再需要向数据库发起 `IN (...)` 查询，故移除 `noteIds` 和 `idListStr` 构建逻辑 |
| **直接构造响应** | 回调签名从 `esResultMap + noteMap` 双映射查找，简化为直接遍历 `esResults` |
| **移除 `folder_id`** | 响应 JSON 中不再包含 `folder_id` 字段 |
| **简化 lambda 捕获** | 移除了不再需要的 `userId`、`keyword` 捕获 |

---

## 四、文档修改详情

### 4.1 `docs/api.md` 第 338 行 `### 2.6 全文搜索笔记`

**修改内容**：响应示例中移除 `folder_id` 字段。

```diff
     {
       "id": 1,
       "title": "我的第一篇笔记",
       "summary": "笔记摘要",
-      "folder_id": 1,
       "created_at": "2025-01-01 12:00:00",
       "updated_at": "2025-01-01 12:00:00",
       "highlight_title": "我的第一篇<mark>笔记</mark>",
```

**未变更内容**：
- 请求参数说明（`keyword`、`isPublic`、`from`、`size`）
- 搜索权重说明（title^3、tags^2、summary^2、content^1）
- 高亮与排序说明
- Elasticsearch 集成说明章节

---

## 五、编译验证

执行编译命令：

```bash
cd calcite/build
cmake .. && make -j$(nproc)
```

**编译结果**：全部通过，零错误、零警告。

构建目标状态：
- `calcite_lib` — 构建成功
- `calcite` — 链接成功
- `calcite_test` — 链接成功

---

## 六、性能影响评估

| 指标 | 修改前 | 修改后 | 变化 |
|------|--------|--------|------|
| 网络 RTT | 1次(ES) + 1次(DB) | 1次(ES) | **减少 50%** |
| 数据库连接占用 | 有 | 无 | **完全消除** |
| 序列化/反序列化开销 | ES JSON + DB Result | ES JSON 单一路径 | **显著降低** |
| 代码复杂度 | 双数据源合并 | 单数据源直出 | **大幅降低** |
| 响应字段 | 含 `folder_id` | 不含 `folder_id` | 精简 |

---

## 七、风险提示与后续建议

1. **ES 与 DB 数据一致性**：本次优化建立在 "ES `_source` 字段与 DB 记录保持一致" 的前提下。若出现同步延迟或同步失败，搜索返回的 `title`/`summary` 可能与 DB 实际值不一致。建议监控 ES 同步队列健康度。
2. **`folder_id` 移除影响**：若前端或其他下游系统依赖 `folder_id` 字段，需同步调整。经确认，该字段在搜索场景无业务用途。
3. **字段兜底**：`EsSearchResult` 中的 `title`、`summary`、`createdAt`、`updatedAt` 均从 ES `_source` 解析。若 `_source` 中某字段缺失，对应 JSON 字段将为空字符串，不会导致崩溃。
