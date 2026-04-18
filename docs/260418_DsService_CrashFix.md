# DsService 崩溃修复报告

## 1. 问题现象

调用 `/api/notes/tags/ai`（`NoteController::generateNoteTagsByAi`）时**偶发崩溃**。

用户反馈在 `DsService::performLlmRequest` 执行后，服务端会不定期 coredump，日志中可能只看到 `crash Here?` 而看不到后续的 `crash Not Here.`。

---

## 2. 根因分析

### 2.1 核心问题：`std::thread` + `detach()` 的伪异步模式

`DsService::performLlmRequest` 原始实现如下（精简）：

```cpp
void DsService::performLlmRequest(
    const std::string& requestJson,
    std::function<void(const TagRecommendationResult&)> callback
) {
    drogon::app().getLoop()->queueInLoop([this, requestJson, callback]() {
        std::thread([requestJson, callback]() {
            CURL* curl = curl_easy_init();
            // ... 同步阻塞执行 curl_easy_perform ...
            drogon::app().getLoop()->queueInLoop(
                [callback, result]() { callback(result); });
        }).detach();
    });
}
```

该实现存在以下几个严重缺陷：

#### (1) 线程生命周期完全失控

每次 API 调用都会 `new` 一个 `std::thread` 并立即 `detach()`。这意味着：
- 服务器无法跟踪这些线程，也无法在关闭时 join 它们；
- 高并发场景下，系统线程数可能瞬间飙升，导致资源耗尽；
- 如果服务器正在重启或关闭，detach 的线程仍在后台运行，访问 `drogon::app().getLoop()` 等全局对象时极易触发** use-after-free / 空指针解引用**，直接 coredump。

#### (2) HTTP 连接断开后 callback 仍被调用

`callback` 是从 `NoteController::generateNoteTagsByAi` 层层传递下来的 Drogon HTTP Response Callback。在 Drogon 中，该 callback 内部持有对底层 `TcpConnection` 的引用。

LLM 请求通常耗时数秒至数十秒。在此期间：
- 客户端可能因超时主动断开 TCP 连接；
- 或者中间代理（Nginx / LB）断开了长连接；
- Drogon 会随之销毁连接对象，但 **detach 的线程对此一无所知**；
- 当线程执行完毕，通过 `queueInLoop` 调用 `callback(result)` 时，callback 内部试图向一个已经销毁的连接写数据，导致**未定义行为（崩溃）**。

#### (3) `queueInLoop` 嵌套 `std::thread` 的反模式

`queueInLoop` 的目的是把任务投递到事件循环线程执行。但 lambda 里又创建了一个 detach 的线程，这使得：
- 事件循环线程只是充当了一个“线程工厂”，没有带来任何 I/O 多路复用的好处；
- 代码复杂度无谓增加，且 `this` 指针在 lambda 中被捕获后，若 `DsService`（或其所在的 `NoteController`）被提前销毁，将产生悬挂引用。

### 2.2 附带发现：`NoteController` 中的 `doInsert` 悬挂引用

在 `NoteController::generateNoteTagsByAi` 中，存在以下代码：

```cpp
std::function<void(size_t)> doInsert = [&](size_t index) {
    // ...
    insertMapper.insert(
        nt,
        [doInsert, index](const drogon_model::calcite::NoteTag &) {
            doInsert(index + 1);   // ← 按引用捕获 doInsert
        },
        // ...
    );
};
doInsert(0);
```

`[doInsert, index]` **按引用捕获**了局部变量 `doInsert`。`insertMapper.insert` 是异步的，其回调在事件循环中稍后执行。当外层的 `deleteBy` success callback 返回后，栈上的 `doInsert` 已经被销毁，后续回调中调用 `doInsert(index + 1)` 即构成 **use-after-free**。

> **说明**：本次修复**未修改** `NoteController.cc`，遵循"尽量只改 `DsService.*`"的原则。该问题需在后续迭代中单独修复（建议将 `doInsert` 改为 `std::shared_ptr<std::function<void(size_t)>>` 并按值捕获）。

---

## 3. 修复方案

### 3.1 思路：用 Drogon 原生 `HttpClient` 替换裸 `curl` + `std::thread`

Drogon 框架本身提供了完善的异步 HTTP 客户端 `drogon::HttpClient`：
- 完全基于事件循环，**不需要任何额外线程**；
- 请求与回调都在同一线程（事件循环线程）中处理，天然线程安全；
- 当底层连接断开时，`HttpClient` 会自动安全地取消请求或调用回调，不会出现向已销毁连接写数据的情况；
- 项目中 `EsClient` 已大量采用该模式，有成熟的参考范例。

### 3.2 代码变更

#### `DsService.h`

```diff
+ #include <drogon/HttpClient.h>

  class DsService {
  private:
-     static constexpr const char* API_URL =
-         "https://api.deepseek.com/chat/completions";
+     static constexpr const char* API_HOST = "https://api.deepseek.com";
+     static constexpr const char* API_PATH = "/chat/completions";

+     drogon::HttpClientPtr client_;
  };
```

#### `DsService.cc`

- **删除** `#include <curl/curl.h>`、`CurlResponse`、`writeCallback`；
- **构造函数**中初始化 `client_`：
  ```cpp
  DsService::DsService()
      : client_(drogon::HttpClient::newHttpClient(API_HOST)) {}
  ```
- **`performLlmRequest` 完全重写**：
  ```cpp
  void DsService::performLlmRequest(
      const std::string& requestJson,
      std::function<void(const TagRecommendationResult&)> callback
  ) {
      auto req = drogon::HttpRequest::newHttpRequest();
      req->setMethod(drogon::Post);
      req->setPath(API_PATH);
      req->addHeader("Authorization", std::string("Bearer ") + API_TOKEN);
      req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
      req->setBody(requestJson);

      client_->sendRequest(req,
          [callback](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
              TagRecommendationResult tagResult;
              if (result != drogon::ReqResult::Ok || !resp) {
                  tagResult.success = false;
                  tagResult.errorMessage = "HTTP request failed";
                  callback(tagResult);
                  return;
              }

              if (resp->getStatusCode() != 200) {
                  tagResult.success = false;
                  tagResult.errorMessage =
                      "HTTP error " + std::to_string(resp->getStatusCode());
                  std::string body = std::string(resp->getBody());
                  if (!body.empty()) {
                      tagResult.errorMessage += ": " + body;
                  }
                  callback(tagResult);
                  return;
              }

              tagResult = parseResponse(std::string(resp->getBody()));
              callback(tagResult);
          },
          API_TIMEOUT);
  }
  ```

### 3.3 修复收益

| 维度 | 修复前 | 修复后 |
|------|--------|--------|
| 线程管理 | `std::thread` + `detach()`，无上限，无法回收 | 零额外线程，全部在事件循环中处理 |
| 连接安全 | HTTP 断开后 callback 仍可能访问已销毁连接 | `HttpClient` 自动处理连接生命周期 |
| 服务器关闭 | detach 线程可能访问已释放全局对象 | 所有请求随 `HttpClient` 安全取消 |
| 代码复杂度 | `curl` C API + 回调 + 线程嵌套 | 纯 Drogon 异步 API，简洁直观 |
| SSL 验证 | 强制关闭 (`VERIFYPEER=0`) | 使用 Drogon 默认配置，更安全 |

---

## 4. 编译验证

```bash
cd calcite/build
cmake ..
make -j$(nproc)
```

结果：
- 主目标 `calcite` **编译通过**；
- `calcite_test` 链接失败属于已有问题（依赖 `calcite_lib` 缺失），与本次修改无关。

---

## 5. 后续建议

1. **修复 `NoteController::generateNoteTagsByAi` 中的 `doInsert` 悬挂引用**：
   ```cpp
   auto doInsert = std::make_shared<std::function<void(size_t)>>();
   *doInsert = [doInsert, ...](size_t index) {
       // ...
       insertMapper.insert(nt,
           [doInsert, index](...) { (*doInsert)(index + 1); },
           ...
       );
   };
   (*doInsert)(0);
   ```

2. **增加 API 超时或重试机制**：当前 `API_TIMEOUT = 60s`，若 DeepSeek 服务波动，可考虑在 `performLlmRequest` 回调中加入 1~2 次指数退避重试。

3. **隐藏 API Token**：当前 `API_TOKEN` 以明文硬编码在头文件中，建议迁移至环境变量或配置文件，避免泄漏风险。

---

**日期**：2026-04-19  
**修改文件**：`calcite/services/DsService.h`、`calcite/services/DsService.cc`
