# EsClient 修改说明文档

> 修改日期：2026-04-18  
> 涉及文件：`calcite/utils/EsClient.h`、`calcite/utils/EsClient.cc`

---

## 一、修改概述

本次修改严格依据 Elasticsearch `notes` 索引的 mapping 结构，对 `EsClient` 进行了以下调整：

1. **修复搜索核心逻辑**：根据 `userId` 的值区分公开笔记与私有笔记查询。
2. **修复字段不匹配问题**：在索引/更新文档时补充 `is_public`、`created_at` 字段。
3. **修复 JSON 语法错误**：修正 `createIndex` 中 mapping 与 settings 之间缺少逗号的问题。
4. **保持向后兼容**：通过为新增参数提供默认值，确保现有调用方（`NoteController`、`OcrController`）无需修改即可编译通过。

---

## 二、搜索函数核心逻辑修改

### 2.1 规则实现

| userId 值 | 查询范围 |
|-----------|----------|
| `userId == 0` | 查询所有 `is_public = true` 的**公开笔记** |
| `userId != 0` | 仅查询 `user_id` 匹配且 `is_public = false` 的**私有笔记** |

### 2.2 生成的 ES Query DSL 示例

**当 `userId == 0` 时（匿名/未登录用户）：**

```json
{
  "from": 0,
  "size": 20,
  "query": {
    "bool": {
      "must": [
        {
          "multi_match": {
            "query": "关键词",
            "fields": ["title^3", "content", "summary^2", "tags^2"],
            "type": "best_fields",
            "fuzziness": "AUTO"
          }
        }
      ],
      "filter": [
        { "term": { "is_public": true } }
      ]
    }
  },
  "highlight": {
    "pre_tags": ["<mark>"],
    "post_tags": ["</mark>"],
    "fields": {
      "title": { "fragment_size": 100, "number_of_fragments": 1 },
      "content": { "fragment_size": 200, "number_of_fragments": 3 },
      "summary": { "fragment_size": 200, "number_of_fragments": 1 }
    }
  },
  "sort": [
    { "_score": { "order": "desc" } }
  ]
}
```

**当 `userId != 0` 时（已登录用户搜索私有笔记）：**

```json
{
  "from": 0,
  "size": 20,
  "query": {
    "bool": {
      "must": [
        {
          "multi_match": {
            "query": "关键词",
            "fields": ["title^3", "content", "summary^2", "tags^2"],
            "type": "best_fields",
            "fuzziness": "AUTO"
          }
        }
      ],
      "filter": [
        { "term": { "user_id": 123 } },
        { "term": { "is_public": false } }
      ]
    }
  },
  ...
}
```

### 2.3 功能支持清单

| 功能 | 说明 |
|------|------|
| **关键词搜索** | `multi_match` 覆盖 `title`、`content`、`summary`、`tags`，其中 `title` 权重 ^3，`summary` 与 `tags` 权重 ^2 |
| **高亮** | 支持 `title`、`content`、`summary` 高亮；`content` 多片段以 ` ... ` 拼接；无 `content` 高亮时回退到 `summary` |
| **分页** | 通过 `from`（起始偏移）和 `size`（每页条数）实现 |
| **相关性排序** | 按 `_score` 降序排列 |

---

## 三、字段匹配性修复

### 3.1 原有实现缺陷

原有 `buildDocumentJson` 生成的文档仅包含以下字段：

- `user_id`
- `title`
- `content`
- `summary`
- `tags`
- `updated_at`

与 `notes` 索引 mapping 对比，**缺少**：

- `is_public`（`boolean`）
- `created_at`（`date`）

### 3.2 修复内容

1. **`buildDocumentJson`**：新增 `is_public` 与 `created_at` 字段，统一使用当前时间填充 `created_at` 和 `updated_at`。
2. **`indexDocument`**：新增可选参数 `bool isPublic = true`，默认值为 `true`，向后兼容。
3. **`updateDocument`**：新增可选参数 `const bool* isPublic = nullptr`，允许部分更新时同步修改公开状态。

### 3.3 索引文档 JSON 示例

```json
{
  "user_id": 42,
  "title": "示例标题",
  "content": "示例内容",
  "summary": "示例摘要",
  "tags": ["tag1", "tag2"],
  "is_public": true,
  "created_at": "2026-04-18 01:49:01",
  "updated_at": "2026-04-18 01:49:01"
}
```

---

## 四、JSON 语法修复

### 4.1 问题定位

`createIndex` 方法中，原始 mapping 字符串在 `"mappings": { ... }` 后直接接 `"settings": { ... }`，**缺少逗号分隔**，导致 Elasticsearch 返回 JSON 解析错误。

### 4.2 修复方式

在 `mappings` 闭合大括号 `}` 后添加逗号 `,`，使其成为合法的 JSON 对象：

```diff
            }
-       }
+       },
        "settings": {
```

---

## 五、编译验证结果

修改后执行编译：

```bash
cd calcite/build
cmake .. && make -j$(nproc)
```

**编译结果：全部通过**，无警告、无错误。

- `calcite_lib`（静态库）构建成功
- `calcite`（主程序）链接成功
- `calcite_test`（测试程序）链接成功

> 说明：由于新增参数均带有默认值，现有调用方（`NoteController`、`OcrController`）无需任何改动即可正常编译。

---

## 六、修改文件清单

| 文件 | 修改类型 |
|------|----------|
| `calcite/utils/EsClient.h` | 接口签名扩展（新增默认参数） |
| `calcite/utils/EsClient.cc` | 实现逻辑修复与字段补充 |

**严禁修改的其他文件**（已确认未改动）：
- `calcite/controllers/NoteController.cc`
- `calcite/controllers/OcrController.cc`
- `calcite/services/*`
