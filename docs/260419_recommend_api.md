# 推荐 API 开发记录

**日期：** 2026年4月19日  
**任务：** 实现基于用户画像的笔记推荐 API  
**开发者：** Kimi Code

---

## 一、任务概述

为 `./calcite/` 项目新增推荐 API：`GET /api/recommend/notes`，支持分页返回个性化推荐的公开笔记列表。

### API 清单

| 接口                 | 方法 | 说明         |
| -------------------- | ---- | ------------ |
| /api/recommend/notes | GET  | 分页推荐笔记 |

---

## 二、技术栈

- **后端框架：** Drogon C++
- **数据库：** MariaDB
- **搜索引擎：** Elasticsearch 7.x+
- **ORM：** Drogon ORM (异步回调模型)

---

## 三、推荐算法设计

### 3.1 用户分类

- **新用户（冷启动）**：最近30天 `user_action` 行为数 < 20
- **老用户（兴趣模型）**：最近30天 `user_action` 行为数 ≥ 20

### 3.2 新用户标签集合 $S_{rec}$

$$S_{rec} = S_{base} \cup S_{search} \cup S_{hot}$$

- **$S_{base}$**：
  - Top3（用户笔记使用最多的标签）
  - Top2（用户最新笔记的标签，去重）
  - 来源：MariaDB (`note_tag` + `tag` + `note` 表联合查询)

- **$S_{search}$**：
  - `search_history` 表中 Top2 搜索关键词
  - SQL：`SELECT query, COUNT(*) AS freq FROM search_history WHERE user_id = ? GROUP BY query ORDER BY freq DESC LIMIT 2`

- **$S_{hot}$**：
  - 全局热门标签 Top3
  - 来源：Elasticsearch 聚合查询（近7天公开笔记 `tags` 字段聚合）

### 3.3 老用户标签集合 $S_{user}$

$$S_{user} = S_{behavior} \cup S_{search} \cup S_{hot}$$

- **$S_{behavior}$**：
  - 基于 `user_tag_stat` 表计算兴趣分数，取 Top5
  - 公式：
    $$tag\_score(t) = (1 \cdot N_{view} + 3 \cdot N_{like} + 7 \cdot N_{collect}) \times e^{-\lambda \cdot \Delta t}, \quad \lambda = 0.1$$
  - $\Delta t$ = 当前时间 − `last_action_time`（单位：天）
  - SQL实现：
    ```sql
    SELECT t.name,
           (uts.view_count * 1 + uts.like_count * 3 + uts.collect_count * 7)
           * EXP(-0.1 * COALESCE(DATEDIFF(NOW(), uts.last_action_time), 30)) AS score
    FROM user_tag_stat uts
    JOIN tag t ON uts.tag_id = t.id
    WHERE uts.user_id = ?
    ORDER BY score DESC
    LIMIT 5
    ```

- **$S_{search}$**、**$S_{hot}$**：同新用户

### 3.4 ES 查询

- **条件**：`is_public = true`，`tags` 在推荐标签集合中（`terms` 查询）
- **排序**：`created_at desc`
- **分页**：`from = (page - 1) * page_size`，`size = page_size`

---

## 四、实现文件清单

### 4.1 EsClient 扩展

**文件：** `./calcite/utils/EsClient.h`  
**文件：** `./calcite/utils/EsClient.cc`

新增接口：

| 方法 | 说明 |
| ---- | ---- |
| `searchByTagsSync` | 按标签列表同步查询公开笔记，支持分页，空标签时查询所有公开笔记 |
| `getHotTagsSync` | 同步获取全局热门标签 TopN（基于近7天公开笔记聚合） |

### 4.2 推荐控制器

**文件：** `./calcite/controllers/RecommendController.h`  
**文件：** `./calcite/controllers/RecommendController.cc`

实现 API：
- **`recommendNotes`**：主入口，鉴权、分页参数解析、新老用户判断、ES查询、响应构造

私有方法：
- **`fetchNewUserTags`**：串行查询 $S_{base}$ → $S_{search}$ → $S_{hot}$，合并去重
- **`fetchOldUserTags`**：串行查询 $S_{behavior}$ → $S_{search}$ → $S_{hot}$，合并去重

### 4.3 核心流程

```
Client ──▶ Token鉴权 ──▶ 解析分页参数 ──▶ 判断新老用户
                                      │
                    ┌─────────────────┴─────────────────┐
                    ▼                                   ▼
              新用户 (<20行为)                      老用户 (≥20行为)
                    │                                   │
                    ▼                                   ▼
           ┌─────────────┐                    ┌─────────────────┐
           │ 1. Top3标签  │                    │ 1. 行为分数Top5  │
           │ 2. Top2最新  │                    │   (view/like/   │
           │ 3. 搜索历史  │                    │    collect加权)  │
           │ 4. 热门标签  │                    │ 2. 搜索历史      │
           └─────────────┘                    │ 3. 热门标签      │
                    │                         └─────────────────┘
                    └─────────────┬─────────────────┘
                                  ▼
                        ┌─────────────────┐
                        │ 合并标签集合 S   │
                        └────────┬────────┘
                                 ▼
                        ┌─────────────────┐
                        │ ES terms 查询    │
                        │ is_public=true   │
                        │ sort: created_at │
                        └────────┬────────┘
                                 ▼
                        ┌─────────────────┐
                        │ 结果为空？       │
                        │ Y: 兜底查询所有  │
                        │    公开笔记      │
                        │ N: 返回标签结果  │
                        └────────┬────────┘
                                 ▼
                        ┌─────────────────┐
                        │ 构造JSON响应     │
                        └─────────────────┘
```

---

## 五、兜底逻辑

1. **标签集合为空**：`searchByTagsSync` 接收空标签时会查询所有公开笔记
2. **ES 标签查询无结果**：若按标签查询返回空且标签非空，自动降级为查询所有公开笔记（按时间倒序）
3. **数据库查询失败**：每个 SQL 步骤独立容错，失败时记录日志并继续执行后续步骤，不会中断推荐流程

---

## 六、API 文档更新

**文件：** `./docs/api.md`

新增内容：
- API 总览表格中增加 `/api/recommend/notes`
- 笔记管理 API 章节中增加 `2.14 推荐笔记 GET /api/recommend/notes`
- 包含请求参数、响应示例、算法说明、兜底逻辑说明

---

## 七、编译验证

### 7.1 编译命令

```bash
cd ./calcite/build
cmake ..
make -j$(nproc)
```

### 7.2 编译结果

```
[ 53%] Built target calcite
[100%] Built target calcite_test
```

编译通过，无警告。

---

## 八、测试示例

### 8.1 获取推荐笔记（第1页，默认10条）

```bash
curl "http://localhost:8080/api/recommend/notes" \
  -H "Authorization: Bearer YOUR_TOKEN"
```

### 8.2 获取推荐笔记（指定分页）

```bash
curl "http://localhost:8080/api/recommend/notes?page=2&page_size=20" \
  -H "Authorization: Bearer YOUR_TOKEN"
```

### 8.3 响应示例

```json
{
  "code": 0,
  "message": "获取推荐成功",
  "data": [
    {
      "id": 42,
      "title": "Elasticsearch 入门指南",
      "summary": "本文介绍 ES 的基本概念与使用...",
      "created_at": "2025-01-01T12:00:00",
      "updated_at": "2025-01-01T12:00:00"
    }
  ]
}
```

---

## 九、注意事项

1. **ES 可用性**：推荐依赖 Elasticsearch，若 ES 不可用则返回空结果（兜底逻辑同样依赖 ES）
2. **冷启动用户**：新用户若没有任何笔记、标签、搜索历史，则完全依赖全局热门标签推荐
3. **行为统计更新**：`user_tag_stat` 表需由笔记浏览/点赞/收藏接口维护，否则老用户推荐效果下降
4. **时间衰减**：$\lambda = 0.1$ 表示约 23 天后行为权重衰减为原来的 10%（$e^{-2.3} \approx 0.1$）
5. **分页限制**：`page_size` 最大 50，超出会被强制限制

---

## 十、后续优化建议

1. **协同过滤**：引入基于物品的协同过滤（ItemCF），利用 `note_like` / `note_collect` 数据
2. **Embedding 召回**：使用向量检索（如 Faiss / ES dense_vector）进行语义相似推荐
3. **实时性优化**：将 `user_tag_stat` 缓存到 Redis，减少推荐时的 SQL 查询
4. **A/B 测试**：支持按用户分组实验不同推荐策略（冷启动策略对比、兴趣衰减系数调参）
5. **去重与多样性**：在 ES 结果中加入 `min_score` 或 `diversity` 策略，避免同一标签下的笔记过度集中
6. **异步预热**：定时任务预计算用户兴趣标签，推荐接口直接读取缓存结果

---

## 十一、文件清单汇总

```
./calcite/
├── utils/
│   ├── EsClient.h              # 修改（新增 searchByTagsSync / getHotTagsSync）
│   └── EsClient.cc             # 修改（新增上述方法实现）
├── controllers/
│   ├── RecommendController.h   # 新增
│   └── RecommendController.cc  # 新增

./docs/
├── api.md                      # 修改（新增 2.14 推荐笔记接口）
└── 260419_recommend_api.md     # 新增（本文档）
```

---

**完成时间：** 2026-04-19 01:38  
**状态：** ✅ 已完成
