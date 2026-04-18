/**
 * DsService.cc
 * LLM-based tag recommendation service implementation
 */

#include "DsService.h"
#include <drogon/drogon.h>
#include <curl/curl.h>
#include <json/json.h>
#include <sstream>
#include <cstdlib>

namespace calcite {
namespace services {

// ===== 可选标签列表（由用户后续填充）=====
static const std::vector<std::string> AVAILABLE_TAGS = {
    // TODO: 在此处粘贴可选标签
};

static const std::string AVAILABLE_TAGS_STR = R"(
Java、Python、C、C++、C#、Go、Rust、JavaScript、TypeScript、PHP、Ruby、Swift、Kotlin、Dart、Lua、Perl、R、Julia、Bash、Shell、PowerShell、Scala、Groovy、Haskell、Clojure、Elixir、Erlang、OCaml、F#、Nim、Crystal、Pascal、Delphi、Assembly、COBOL、Fortran、Lisp、Prolog、Raku、Tcl、VBScript、AutoHotkey、CoffeeScript、PureScript、ActionScript、HaXe、Smalltalk、Ada、D、Vala、Forth、Factor、Modula、Oberon、Jython、JRuby、IronPython、IronRuby、Ceylon、Fantom、GML、AngelScript、Rust宏、泛型编程、元编程、函数式、面向对象、并发编程、异步编程、协程、

HTML、HTML5、CSS、CSS3、Sass、SCSS、Less、Stylus、PostCSS、TailwindCSS、UnoCSS、Bootstrap、Bulma、ElementUI、NaiveUI、Vuetify、AntD、MUI、ChakraUI、React、Vue、Vue3、Svelte、SolidJS、Qwik、Next.js、Nuxt、Astro、Remix、Redux、Pinia、Mobx、ReactQuery、SWR、Vite、Webpack、Rollup、Esbuild、Babel、ESLint、Prettier、Jest、Vitest、Cypress、Playwright、Canvas、SVG、WebGL、Three.js、WebRTC、WebSocket、PWA、WebAssembly、微前端、模块联邦、WebComponent、ShadowDOM、跨端、响应式、移动端适配、Rem布局、Viewport、HTTP、HTTPS、Fetch、Axios、GraphQL、Apollo、URQL、路由、状态管理、性能优化、代码分割、

Spring、SpringBoot、SpringCloud、MyBatis、JPA、Hibernate、Quarkus、Vert.x、Servlet、Tomcat、Jetty、Undertow、Nginx、OpenResty、JWT、OAuth2、Swagger、Knife4j、RESTful、gRPC、Thrift、Dubbo、RocketMQ、Kafka、RabbitMQ、Pulsar、Netty、Reactor、RxJava、分布式锁、分布式事务、Seata、Saga、本地消息表、最大努力通知、AOP、IOC、DI、过滤器、拦截器、网关、Sentinel、Hystrix、限流、熔断、降级、重试、超时、幂等、链路追踪、SkyWalking、Jaeger、ELK、MinIO、FastDFS、JSR380、Validation、异步任务、定时任务、Quartz、XXL-Job、多线程、线程池、JVM、JVM调优、GC、类加载、字节码、热部署、

MySQL、PostgreSQL、Oracle、SQLServer、SQLite、MariaDB、TiDB、OceanBase、MongoDB、Redis、RedisCluster、Sentinel、Memcached、Elasticsearch、Solr、ClickHouse、Doris、StarRocks、InfluxDB、Prometheus、TDengine、HBase、Cassandra、Neo4j、ArangoDB、CockroachDB、Yugabyte、Etcd、ZooKeeper、SQL、NoSQL、NewSQL、索引、B+树、LSM树、MVCC、事务、ACID、CAP、BASE、分库分表、ShardingJDBC、MyCat、主从复制、读写分离、Binlog、Redo、Undo、WAL、慢查询、Explain、执行计划、备份恢复、时序数据库、图数据库、向量数据库、Milvus、Weaviate、FAISS、Pinecone、Chroma、RedisJSON、RediSearch、RediSQL、Kvrocks、Pika、LevelDB、RocksDB、

Linux、CentOS、Ubuntu、Debian、Shell、Awk、Sed、Grep、Systemd、Crontab、Iptables、Firewalld、SSH、Rsync、LVM、RAID、TCP、UDP、DNS、DHCP、Docker、K8s、Kubernetes、Helm、Istio、Harbor、Jenkins、GitLabCI、GitHubActions、Ansible、Terraform、Vault、OpenStack、VMware、KVM、Xen、Zabbix、Grafana、Loki、ELK、Nagios、SNMP、高可用、负载均衡、蓝绿发布、金丝雀发布、DevOps、SRE、CI/CD、监控告警、日志收集、链路监控、安全加固、SELinux、证书、TLS、SSL、OpenSSL、NFS、Samba、SFTP、磁盘IO、CPU优化、内存管理、网络调优、

Hadoop、HDFS、YARN、Hive、Spark、Flink、Kafka、Pulsar、HBase、Presto、Impala、Sqoop、Flume、DataX、SeaTunnel、Iceberg、Hudi、DeltaLake、Airflow、DolphinScheduler、ETL、数据仓库、数据湖、批处理、流处理、机器学习、深度学习、NLP、大模型、LLM、Transformer、BERT、GPT、RAG、LoRA、QLoRA、PEFT、Prompt、Embedding、TensorFlow、PyTorch、ONNX、TensorRT、CUDA、MLOps、模型部署、量化蒸馏、微调、预训练、OCR、ASR、TTS、AIGC、StableDiffusion、YOLO、ResNet、ViT、数据挖掘、特征工程、聚类、分类、回归、推荐系统、用户画像、

单元测试、集成测试、接口测试、性能测试、安全测试、Selenium、Appium、JMeter、Postman、SonarQube、Git、SVN、Maven、Gradle、Nexus、Jira、Confluence、VSCode、IDEA、PyCharm、Navicat、DBeaver、Xshell、MobaXterm、WSL、Markdown、Mermaid、XMind、ProcessOn、DrawIO、DDD、TDD、设计模式、单例、工厂、策略、装饰、观察者、代理、门面、适配器、微服务、领域驱动、代码规范、重构、CodeReview、架构设计、高并发、高可用、

考研、考公、四六级、雅思、托福、GRE、专四专八、教师资格证、会计证、法考、建筑师、软考、PMP、网络工程师、软件设计师、信息安全、高数、线性代数、概率论、统计学、物理、化学、生物、历史、地理、政治、语文、英语、日语、韩语、德语、法语、写作、阅读、笔记方法、思维导图、记忆力、专注力、时间管理、自律、错题本、刷题、背诵、作文、素材、论文、文献、开题报告、查重、降重、答辩、面试、简历、职场沟通、汇报技巧、演讲、逻辑思维、批判性思维、知识管理、学习复盘、效率工具、外语口语、听力、语法、词汇、

职场、实习、转正、跳槽、薪资、绩效、KPI、OKR、复盘、周报、月报、项目管理、需求分析、产品思维、用户体验、沟通技巧、情绪管理、抗压、领导力、团队协作、会议管理、时间规划、副业、自媒体、跨境、电商、运营、增长、数据分析、商业思维、理财、基金、股票、保险、房产、储蓄、消费观、人脉、社交、礼仪、形象管理、表达能力、谈判、领导力、执行力、习惯养成、目标管理、个人品牌、

健康、运动、健身、跑步、瑜伽、普拉提、篮球、足球、羽毛球、游泳、睡眠、作息、饮食、减脂、增肌、养生、茶饮、煲汤、家常菜、烘焙、咖啡、奶茶、护肤、美妆、发型、穿搭、整理收纳、家居、绿植、宠物、摄影、旅行、自驾游、露营、徒步、城市打卡、电影、剧集、综艺、音乐、乐器、吉他、钢琴、手工、绘画、书法、手账、日记、情绪疏导、心理调节、冥想、放松、

游戏、桌游、卡牌、剧本杀、密室、动漫、漫画、小说、网文、诗词、国学、哲学、星座、塔罗、园艺、钓鱼、骑行、滑雪、冲浪、数码、耳机、键盘、摄影后期、Vlog、剪辑、配音、

低代码、无代码、小程序、快应用、鸿蒙、Flutter、ReactNative、UniApp、Taro、物联网、嵌入式、单片机、树莓派、Arduino、ROS、机器人、5G、边缘计算、云计算、阿里云、腾讯云、华为云、AWS、Azure、虚拟化、容器、Serverless、RPC、API设计、接口文档、国际化、i18n、本地化、SEO、SEM、爬虫、逆向、加解密、国密、SM2、SM3、SM4、WAF、防火墙、渗透测试、漏洞扫描、CVE、应急响应、等保、隐私合规、GDPR、等保2.0、数据安全、容灾备份、跨平台、桌面应用、Electron、WPF、WinForm、Qt、GTK、游戏开发、Unity、Unreal、Cocos、Shader、动画、特效、建模、Blender、3DMax、CAD、UI设计、UX设计、Figma、Sketch、PS、AI、AE、PR、配色、排版、字体、图标、动效、

)";

// Response structure for curl operations
struct CurlResponse {
    std::string data;
    long httpCode = 0;
};

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    CurlResponse* response = static_cast<CurlResponse*>(userp);
    size_t totalSize = size * nmemb;
    response->data.append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

DsService::DsService() = default;
DsService::~DsService() = default;

std::string DsService::buildPrompt(const std::string& noteContent) {
    std::ostringstream oss;
    oss << "任务：为笔记从标签库中推选** exactly 5 个**标签。\n"
        << "要求：\n"
        << "- 必须从下方标签列表中选择，禁止自创\n"
        << "- 优先匹配：技术方向、知识点、用途、场景\n"
        << "- 结果只输出 5 个标签，逗号分隔，无其他文字\n"
        << "\n"
        << "笔记内容：\n"
        << "{````{" << noteContent << "}````}\n"
        << "\n"
        << "可选标签：\n"
        << "[{{";

    oss<< AVAILABLE_TAGS_STR;

    oss << "}}]";
    return oss.str();
}

std::string DsService::buildRequestJson(const std::string& prompt) {
    Json::Value root;
    root["model"] = API_MODEL;
    root["stream"] = false;

    Json::Value messages(Json::arrayValue);

    Json::Value systemMsg;
    systemMsg["role"] = "system";
    systemMsg["content"] = "You are a note tag classification assistant";
    messages.append(systemMsg);

    Json::Value userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = prompt;
    messages.append(userMsg);

    root["messages"] = messages;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

TagRecommendationResult DsService::parseResponse(const std::string& responseData) {
    TagRecommendationResult result;

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;

    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(responseData.c_str(),
                       responseData.c_str() + responseData.size(),
                       &root, &errors)) {
        result.success = false;
        result.errorMessage = "Failed to parse JSON response: " + errors;
        return result;
    }

    // Check for API-level error
    if (root.isMember("error") && !root["error"].isNull()) {
        result.success = false;
        if (root["error"].isMember("message")) {
            result.errorMessage = root["error"]["message"].asString();
        } else {
            result.errorMessage = "DeepSeek API returned an error";
        }
        return result;
    }

    // Extract content from choices
    if (!root.isMember("choices") || !root["choices"].isArray() ||
        root["choices"].empty()) {
        result.success = false;
        result.errorMessage = "Invalid response: no choices found";
        return result;
    }

    const Json::Value& firstChoice = root["choices"][0];
    if (!firstChoice.isMember("message") ||
        !firstChoice["message"].isMember("content")) {
        result.success = false;
        result.errorMessage = "Invalid response: missing message content";
        return result;
    }

    std::string content = firstChoice["message"]["content"].asString();

    // Parse comma-separated tags
    std::istringstream tagStream(content);
    std::string tag;
    while (std::getline(tagStream, tag, ',')) {
        // Trim whitespace
        size_t start = tag.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            continue;
        }
        size_t end = tag.find_last_not_of(" \t\n\r");
        std::string trimmed = tag.substr(start, end - start + 1);
        if (!trimmed.empty()) {
            result.tags.push_back(trimmed);
        }
    }

    result.success = !result.tags.empty();
    if (!result.success) {
        result.errorMessage = "LLM returned empty tag list";
    }

    return result;
}

void DsService::performLlmRequest(
    const std::string& requestJson,
    std::function<void(const TagRecommendationResult&)> callback
) {
    std::cout<<"Must be here."<<'\n';
    drogon::app().getLoop()->queueInLoop([this, requestJson, callback]() {
        std::thread([requestJson, callback]() {
            CURL* curl = curl_easy_init();
            if (!curl) {
                TagRecommendationResult result;
                result.success = false;
                result.errorMessage = "Failed to initialize CURL";
                drogon::app().getLoop()->queueInLoop(
                    [callback, result]() { callback(result); });
                return;
            }

            std::string authHeader = "Authorization: Bearer ";
            authHeader += API_TOKEN;

            CurlResponse response;

            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, authHeader.c_str());

            curl_easy_setopt(curl, CURLOPT_URL, API_URL);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestJson.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, API_TIMEOUT);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

            CURLcode res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.httpCode);

            TagRecommendationResult result;
            if (res != CURLE_OK) {
                result.success = false;
                result.errorMessage =
                    std::string("CURL error: ") + curl_easy_strerror(res);
            } else if (response.httpCode == 200) {
                result = parseResponse(response.data);
            } else {
                result.success = false;
                result.errorMessage =
                    "HTTP error " + std::to_string(response.httpCode);
                if (!response.data.empty()) {
                    result.errorMessage += ": " + response.data;
                }
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            drogon::app().getLoop()->queueInLoop(
                [callback, result]() { callback(result); });

                std::cout<<"Must be here. result success: "<<result.success<<", errorMessage: "<<result.errorMessage<<'\n';
        }).detach();
    });
}

void DsService::recommendTags(
    const std::string& noteContent,
    std::function<void(const TagRecommendationResult&)> callback
) {
    if (noteContent.empty()) {
        TagRecommendationResult result;
        result.success = false;
        result.errorMessage = "Empty note content";
        callback(result);
        return;
    }

    std::string prompt = buildPrompt(noteContent);
    std::string requestJson = buildRequestJson(prompt);
    performLlmRequest(requestJson, callback);
}

} // namespace services
} // namespace calcite
