#include "./models/EsClient.h"
#include <iostream>
#include <drogon/drogon.h>
#include <thread>
#include <chrono>

using namespace std;
using namespace drogon;

int main() {
    std::cout << "ElasticSearch Client Example" << std::endl;

    // 创建 ES 客户端（连接到本地 Elasticsearch）
    EsClient es("http://localhost:9200");
    std::cout << "ES Client created" << std::endl;

    // 准备插入的笔记数据
    string noteJson = R"({
        "note_id": 1001,
        "user_id": 123,
        "title": "Elasticsearch 学习",
        "content": "今天学习了ES全文检索",
        "tags": ["学习","技术"],
        "created_at": "2026-04-02T12:00:00Z"
    })";

    std::string index("notes");
    std::string id("1001");

    // 用于通知主线程完成
    std::promise<void> allDone;
    std::future<void> allDoneFuture = allDone.get_future();

    // 在单独的线程中运行事件循环
    std::thread eventLoopThread([&]() {
        // 启动事件循环
        drogon::app().getLoop()->queueInLoop([&]() {
            // 1. 插入文档
            std::cout << "正在插入文档..." << std::endl;
            es.indexDocument(index, id, noteJson);
        });

        // 1秒后执行搜索
        drogon::app().getLoop()->runAfter(1.0, [&]() {
            // 2. 搜索笔记
            std::string content("学习");
            std::cout << "正在搜索 '" << content << "'..." << std::endl;

            es.search(index, content, [](const std::string& result) {
                if (!result.empty()) {
                    cout << "搜索结果：" << endl;
                    cout << result << endl;
                } else {
                    cout << "搜索失败或未找到结果" << endl;
                }

                // 所有操作完成，退出事件循环
                drogon::app().getLoop()->queueInLoop([]() {
                    drogon::app().quit();
                });
            });
        });

        // 启动事件循环
        drogon::app().run();
        allDone.set_value();
    });

    // 等待所有操作完成
    allDoneFuture.wait();
    eventLoopThread.join();

    std::cout << "程序执行完成！" << std::endl;

    return 0;
}
