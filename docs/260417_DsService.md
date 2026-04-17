已完成 `EsService` 的实现，编译通过。

### 文件结构
```
calcite/services/
├── EsService.h
└── EsService.cc
```

### 接口说明

**`EsService.h`** 中暴露的函数：

```cpp
void recommendTags(
    const std::string& noteContent,
    std::function<void(const TagRecommendationResult&)> callback
);
```

- `noteContent`: 笔记正文内容
- `callback`: 异步回调，参数为 `TagRecommendationResult`
  - `success`: 是否成功
  - `tags`: 解析出的标签列表（按逗号分隔）
  - `errorMessage`: 失败时的错误信息

### 需要用户补充的位置

在 **`EsService.cc`** 第 18-20 行，我已预留了可选标签列表的位置：

```cpp
// ===== 可选标签列表（由用户后续填充）=====
static const std::vector<std::string> AVAILABLE_TAGS = {
    // TODO: 在此处粘贴可选标签
};
```

### 其他关键细节

- **API Key 来源**：从环境变量 `DEEPSEEK_API_KEY` 读取（样例命令中的 `${DEEPSEEK_API_KEY}`）。
- **异步模型**：仿照 `OcrService`，在后台线程执行 curl 请求，完成后通过 `drogon::app().getLoop()->queueInLoop()` 回调到主循环。
- **Prompt**：完全按照你提供的模板拼接，将 `noteContent` 填入 `{````{...}````}` 区块，将 `AVAILABLE_TAGS` 填入 `[{{...}}]` 区块。
- **响应解析**：从 DeepSeek 返回的 JSON 中提取 `choices[0].message.content`，按逗号分割并 trim 空格后存入 `tags`。