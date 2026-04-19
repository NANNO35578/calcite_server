#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include "/home/usr24/github/calcite_server/calcite/utils/EsClient.h"
#include <iostream>
#include <future>
#include <thread>
#include <chrono>

using namespace drogon;
using namespace std::chrono;

// 只用最简单的 cout 打印，绝对不会报错！
#define LOG_TEST(msg) \
    std::cout << "\033[32m[TEST] " << msg << "\033[0m" << std::endl

DROGON_TEST(BasicTest)
{
    auto esClient = std::make_shared<calcite::utils::EsClient>();

    auto prom = std::make_shared<std::promise<void>>();
    auto fut = prom->get_future();


    esClient->search(1, true, "Java", [prom](const std::vector<calcite::utils::EsSearchResult> &results, const std::map<std::string, int> &tagHitCounts) {
        LOG_TEST("结果数量：" + std::to_string(results.size()));

        for (const auto &res : results) {
            LOG_TEST("笔记ID：" + std::to_string(res.noteId) + " 标题：" + res.title);
        }
        LOG_TEST("标签命中统计：");
        for (const auto &tagCount : tagHitCounts) {
            LOG_TEST("标签：" + tagCount.first + " 命中次数：" + std::to_string(tagCount.second));
        }

        prom->set_value();
    });

    // 等待
    if (fut.wait_for(5s) == std::future_status::timeout) {
        LOG_TEST("ERROR：搜索超时！");
        CHECK(false);
    } else {
        LOG_TEST("ES 测试完成 ✅");
        CHECK(true);
    }
}

// 安全 main，彻底解决段错误
int main(int argc, char** argv)
{

    std::promise<void> p1;
    std::thread thr([&]() {
        app().getLoop()->queueInLoop([&]() {
            p1.set_value();
        });
        app().run();
    });

    p1.get_future().wait();
    int status = test::run(argc, argv);

    // 安全退出
    std::this_thread::sleep_for(milliseconds(200));
    app().quit();
    thr.join();

    LOG_TEST("程序正常退出，无崩溃 ✅");
    return status;
}