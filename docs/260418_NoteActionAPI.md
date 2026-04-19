# 260418 笔记互动 API 开发记录

## 概述

本次操作为 Calcite 项目新增了 5 个笔记互动相关的 RESTful API，涵盖浏览、点赞、收藏及取消操作。所有实现均遵循现有代码风格，仅修改 `NoteController.*` 与 `api.md` 文件。

## 新增 API 清单

| 接口                   | 方法     | 说明       | 参数      |
| ---------------------- | -------- | ---------- | --------- |
| `/api/note/view`       | POST     | 浏览笔记   | note_id   |
| `/api/note/like`       | POST     | 点赞笔记   | note_id   |
| `/api/note/collect`    | POST     | 收藏笔记   | note_id   |
| `/api/notes/like`      | DELETE   | 取消点赞   | note_id   |
| `/api/notes/collect`   | DELETE   | 取消收藏   | note_id   |

## 实现细节

### `/api/note/view` POST 浏览笔记

1. **鉴权**：通过 `Authorization: Bearer {token}` 验证用户身份。
2. **权限检查**：查询 `note` 表，确认笔记存在且当前用户有权访问（笔记属于当前用户或笔记为公开）。
3. **记录行为**：向 `user_action` 表插入记录，`action_type = 1`（view）。
4. **更新笔记计数**：执行 `UPDATE note SET view_count = view_count + 1 WHERE id = ?`。
5. **更新标签统计**：
   - 查询 `note_tag` 获取该笔记的所有 `tag_id`。
   - 批量执行 `INSERT INTO user_tag_stat (...) VALUES (...), (...) ON DUPLICATE KEY UPDATE view_count = view_count + 1, last_action_time = NOW()`。

### `/api/note/like` POST 点赞笔记

1. **鉴权与权限检查**：同上。
2. **幂等性处理**：使用 `INSERT IGNORE INTO note_like (user_id, note_id) VALUES (?, ?)`，若 `affectedRows == 0` 则返回"已点赞过该笔记"。
3. **记录行为**：向 `user_action` 表插入记录，`action_type = 2`（like）。
4. **更新笔记计数**：`UPDATE note SET like_count = like_count + 1 WHERE id = ?`。
5. **更新标签统计**：批量 `INSERT ... ON DUPLICATE KEY UPDATE like_count = like_count + 1`。

### `/api/note/collect` POST 收藏笔记

逻辑与点赞完全一致，区别如下：
- 操作表：`note_collect`
- `user_action.action_type = 3`（collect）
- 更新字段：`note.collect_count` 与 `user_tag_stat.collect_count`
- 收藏行为在推荐算法中权重最高。

### `/api/notes/like` DELETE 取消点赞

1. **鉴权**：验证 Token。
2. **参数获取**：支持 Query String (`?note_id=1`) 或 JSON Body 传递 `note_id`。
3. **删除记录**：执行 `DELETE FROM note_like WHERE user_id = ? AND note_id = ?`，若 `affectedRows == 0` 则返回"未点赞过该笔记"。
4. **更新笔记计数**：`UPDATE note SET like_count = GREATEST(like_count - 1, 0) WHERE id = ?`，防止计数为负。
5. **更新标签统计**：`UPDATE user_tag_stat SET like_count = GREATEST(like_count - 1, 0) WHERE user_id = ? AND tag_id IN (...)`。

### `/api/notes/collect` DELETE 取消收藏

逻辑与取消点赞一致，操作表和目标字段对应为 `note_collect` 与 `collect_count`。

## 涉及文件

- `calcite/controllers/NoteController.h`：新增方法声明、路由注册、辅助方法声明。
- `calcite/controllers/NoteController.cc`：实现 5 个 API 及 2 个辅助方法（`getNoteTagIds`、`batchUpdateTagStat`）。
- `docs/api.md`：补充 API 文档。
- `docs/260418_NoteActionAPI.md`：本操作记录。

## 辅助方法说明

### `getNoteTagIds`

通过 Drogon ORM 查询 `note_tag` 表，异步返回指定 `note_id` 的所有 `tag_id` 列表。若查询失败则返回空列表并记录错误日志，不影响主流程。

### `batchUpdateTagStat`

根据 `actionType`（view/like/collect）和 `isIncrement`（true/false）构建并执行原生 SQL，批量更新 `user_tag_stat` 表：
- **增量操作**：使用 `INSERT ... ON DUPLICATE KEY UPDATE` 实现原子插入或累加。
- **减量操作**：使用 `UPDATE ... SET field = GREATEST(field - 1, 0)` 实现安全递减。

## 编译验证

```bash
cd calcite/build
cmake ..
make -j4
```

编译通过，无警告，可执行文件生成正常。

## 设计考量

1. **原子性**：笔记计数更新采用原生 SQL（`view_count + 1` / `GREATEST(like_count - 1, 0)`），避免并发下的读写竞争问题。
2. **容错性**：`user_action` 写入失败或 `user_tag_stat` 更新失败均记录日志，但不阻塞主流程，仍向客户端返回成功（行为已记录于代码注释中）。
3. **幂等性**：点赞/收藏使用 `INSERT IGNORE` 保证幂等；取消操作通过 `affectedRows` 判断是否存在记录。
4. **权限控制**：浏览/点赞/收藏需验证笔记访问权限（私有笔记仅限作者）；取消操作仅需验证 Token，不重复校验笔记权限。
