#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include "/home/usr24/github/calcite_server/calcite/utils/EsClient.h"

DROGON_TEST(BasicTest)
{
    // Add your tests here

    calcite::utils::EsClient esClient;
    std::promise<void> prom;  // 用来等待异步完成
    auto fut = prom.get_future();

    // 测试索引文档
    esClient.indexDocument(1, 1, "Test Note");

    // ==========================================
    // 关键：阻塞等待异步回调完成
    // ==========================================
    fut.get();
    CHECK(true);
}

int main(int argc, char** argv) 
{
    using namespace drogon;

    std::promise<void> p1;
    std::future<void> f1 = p1.get_future();

    // Start the main loop on another thread
    std::thread thr([&]() {
        // Queues the promise to be fulfilled after starting the loop
        app().getLoop()->queueInLoop([&p1]() { p1.set_value(); });
        app().run();
    });

    // The future is only satisfied after the event loop started
    f1.get();
    int status = test::run(argc, argv);

    // Ask the event loop to shutdown and wait
    app().getLoop()->queueInLoop([]() { app().quit(); });
    thr.join();
    return status;
}
