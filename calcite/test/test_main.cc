#define DROGON_TEST_MAIN
#include <drogon/drogon_test.h>
#include <drogon/drogon.h>
#include "../services/DsService.h"

DROGON_TEST(BasicTest)
{
    // Add your tests here
    using namespace calcite::services;

    DsService dsService;
    std::promise<void> prom;  // 用来等待异步完成
    auto fut = prom.get_future();

    // 你的笔记内容
    std::string note = R"(
**本质**：**AI 执行引擎**，不是大模型本身，而是连接大模型与电脑系统的 “桥梁”.
- 不同于常规*对话式*AI

**SKILL**: 一个md文档, 相当于提示词中固定部分, 比如"写算法题用C++"

OpenClaw 体系里的核心术语（如 **Skill、MCP、Agent、Tool、Plugin** 等）分工非常明确，我给你一次性讲透，全部用**人话+类比+定位**讲清楚，方便你快速理解。

---

## 一、最核心：MCP、Skill、Tool、Agent、Plugin
### 1. **MCP (Model Context Protocol)**
**定位**：AI 与外部世界的 **统一接口协议 / 万能插座 / 神经系统**
**简单理解**：AI 的 **USB-C**，让 AI 能安全、标准地连接文件、系统、API、数据库、软件。
- 不是能力本身，是 **连接规范**
- 解决：**怎么接外部工具**
- 作用：让 OpenClaw 能调用本地/远程服务（读文件、发邮件、查网页、控软件）

**类比**：
MCP = 电线 + 插座 → 负责通电、传输信号，不负责干活。

---

### 2. **Skill（技能）**
**定位**：AI 的 **可执行任务单元 / 工作流程 / 专业手册**
**简单理解**：一组 **标准化操作步骤 + 工具调用逻辑**，告诉 AI **什么时候、怎么用工具完成任务**。
- 本质：**任务封装**（不是底层动作）
- 形式：`SKILL.md`（说明）+ 代码（执行）
- 解决：**会不会做、怎么组合工具**

**例子**：
- `github-manager`：管理 GitHub（建 issue、发 PR、代码审查）
- `daily-brief`：每日简报（查日历 + 读邮件 + 总结）
- `code-review`：代码审查（读文件 + 静态检查 + 评论）

**类比**：
Skill = **菜谱 / 操作手册** → 告诉你“先做什么、后做什么、用什么工具”。

---

### 3. **Tool（工具）**
**定位**：**原子能力 / 底层动作 / 函数**
**简单理解**：AI 能直接调用的 **最小功能单元**（“能不能做”）。
- 例子：
  - `file_read`、`file_write`
  - `web_search`、`http_request`
  - `shell_exec`、`send_email`
- 来源：
  - 原生 Tool（OpenClaw 内置）
  - MCP Tool（外部 MCP 服务器提供）

**类比**：
Tool = **菜刀、锅、打火机** → 最基础的“干活工具”。

---

### 4. **Agent（智能体）**
**定位**：**AI 大脑 / 数字员工 / 指挥中心**
**简单理解**：接收任务、做决策、**调用 Skill/Tool 完成目标**的主体。
- 有：名字、角色（如：程序员、分析师）、记忆、工作区
- 负责：任务拆解 → 规划 → 调度 Skill → 执行 → 反馈

**类比**：
Agent = **厨师长 / 项目经理** → 懂业务、会安排、下指令。

---

### 5. **Plugin（插件）**
**定位**：**能力扩展包 / 模块**
**简单理解**：把一组 **Tool/Skill** 打包，**安装进 OpenClaw** 的方式。
- 解决：**怎么把新能力装到系统里**

**类比**：
Plugin = **APP 安装包** → 一键安装一组功能。
    )";

    // 调用服务
    dsService.recommendTags(note, [&](const TagRecommendationResult& result) {
        // ==========================================
        // 现在这里一定会执行！一定会输出！
        // ==========================================
        std::cout << "\n=====================================" << std::endl;
        std::cout << "Success: " << std::boolalpha << result.success << std::endl;
        if (!result.success) {
            std::cout << "Error: " << result.errorMessage << std::endl;
        } else {
            std::cout << "推荐标签：";
            for (const auto& tag : result.tags) {
                std::cout << tag << " ";
            }
            std::cout << "\n=====================================" << std::endl;
        }

        prom.set_value(); // 通知测试：我完成了！
    });

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
