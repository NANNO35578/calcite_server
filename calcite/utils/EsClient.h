#pragma once
#include <drogon/HttpClient.h>
#include <string>
#include <functional>
#include <vector>
#include <json/json.h>

namespace calcite {
namespace utils {

/**
 * Elasticsearch 搜索结果结构
 */
struct EsSearchResult {
    int64_t noteId;              // 笔记ID（来自ES文档_id）
    std::string title;           // 原标题（从_source取）
    std::string summary;         // 摘要（从_source取）
    std::string createdAt;       // 创建时间
    std::string updatedAt;       // 更新时间
    std::string highlightTitle;  // 高亮标题
    std::string highlightContent;// 高亮内容片段
    float score;                 // 匹配分数
};

/**
 * Elasticsearch 客户端
 * 用于与ES进行交互，支持索引、更新、删除、搜索等操作
 */
class EsClient {
public:
    /**
     * 构造函数
     * @param host ES服务地址，默认 http://localhost:9200
     * @param indexName 索引名称，默认 notes
     */
    EsClient(const std::string& host = "http://localhost:9200",
             const std::string& indexName = "notes");

    /**
     * 异步索引文档（创建或替换）
     * @param noteId 笔记ID（作为ES文档ID）
     * @param userId 用户ID
     * @param title 标题
     * @param content 内容
     * @param summary 摘要
     * @param tags 标签列表
     * @param isPublic 是否公开，默认 false
     */
    void indexDocument(int64_t noteId,
                       int64_t userId,
                       const std::string& title,
                       const std::string* content=nullptr,
                       const std::string* summary=nullptr,
                       const std::vector<std::string>& tags=std::vector<std::string>(),
                       bool isPublic = false);

    /**
     * 异步更新文档（部分更新）
     * @param noteId 笔记ID
     * @param title 标题（可选）
     * @param content 内容（可选）
     * @param summary 摘要（可选）
     * @param tags 标签列表（可选）
     * @param isPublic 是否公开（可选）
     */
    void updateDocument(int64_t noteId,
                        const std::string* title = nullptr,
                        const std::string* content = nullptr,
                        const std::string* summary = nullptr,
                        const std::vector<std::string>& tags = std::vector<std::string>(),
                        const bool* isPublic = nullptr);

    /**
     * 异步删除文档
     * @param noteId 笔记ID
     */
    void deleteDocument(int64_t noteId);

    /**
     * 全文搜索（异步）
     * @param userId 用户ID（用于权限过滤）
     * @param isPublic 是否查询自己笔记
     * @param keyword 搜索关键词
     * @param callback 回调函数，返回搜索结果列表
     * @param from 分页起始位置
     * @param size 每页大小
     */
    void search(int64_t userId,
                bool isPublic,
                const std::string& keyword,
                std::function<void(const std::vector<EsSearchResult>&)> callback,
                int from = 0,
                int size = 20);

    /**
     * 同步搜索（用于简单场景）
     * @param userId 用户ID
     * @param isPublic 是否查询自己笔记
     * @param keyword 搜索关键词
     * @param from 分页起始位置
     * @param size 每页大小
     * @param timeoutMs 超时时间（毫秒）
     * @return 搜索结果列表
     */
    std::vector<EsSearchResult> searchSync(int64_t userId,
                                           bool isPublic,
                                           const std::string& keyword,
                                           int from = 0,
                                           int size = 20,
                                           int timeoutMs = 5000);

    /**
     * 检查ES是否可用
     * @return 是否可用
     */
    bool ping(int timeoutMs = 3000);

    /**
     * 创建索引（如果不存在）
     * @return 是否成功
     */
    bool createIndex(int timeoutMs = 5000);

private:
    drogon::HttpClientPtr client_;
    std::string indexName_;

    /**
     * 构建文档JSON
     */
    std::string buildDocumentJson(int64_t userId,
                                  const std::string* title,
                                  const std::string* content,
                                  const std::string* summary,
                                  const std::vector<std::string>& tags,
                                  bool isPublic);

    /**
     * 解析搜索结果
     */
    std::vector<EsSearchResult> parseSearchResult(const std::string& jsonResponse);

    /**
     * 转义JSON字符串
     */
    std::string escapeJson(const std::string* str);
};

} // namespace utils
} // namespace calcite
