#include "EsClient.h"
#include <drogon/HttpRequest.h>
#include <future>
#include <chrono>
#include <sstream>

using namespace drogon;

namespace calcite {
namespace utils {

EsClient::EsClient(const std::string& host, const std::string& indexName)
    : client_(drogon::HttpClient::newHttpClient(host)),
      indexName_(indexName) {
}

std::string EsClient::escapeJson(const std::string* str) {
    if(!str) return "";
    
    std::string result;
    result.reserve(str->size());
    for (char c : *str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c >= 0x20 && c <= 0x7E) {
                    result += c;
                } else {
                    // 处理UTF-8字符
                    result += c;
                }
        }
    }
    return result;
}

std::string EsClient::buildDocumentJson(int64_t userId,
                                        const std::string* title,
                                        const std::string* content,
                                        const std::string* summary,
                                        const std::vector<std::string>& tags,
                                        bool isPublic) {
    std::ostringstream oss;

    // ✅ 修复：使用 trantor 官方接口生成 ES 标准日期格式（UTC 时间）
    auto now = trantor::Date::now();
    char timeBuf[32] = {0};
    now.toCustomFormattedString("%Y-%m-%dT%H:%M:%S", timeBuf, sizeof(timeBuf));
    std::string nowFormat = timeBuf;

    // std::cout << "Current time for ES document: " << nowFormat << std::endl;

    oss << "{";
    oss << "\"user_id\":" << userId << ",";
    oss << "\"title\":\"" << escapeJson(title) << "\",";
    oss << "\"content\":\"" << escapeJson(content) << "\",";
    oss << "\"summary\":\"" << escapeJson(summary) << "\",";
    
    oss << "\"tags\":[";
    if (!tags.empty()) {
      for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(&(tags[i])) << "\"";
      }
    }
    oss << "],";

    oss << "\"is_public\":" << (isPublic ? "true" : "false") << ",";
    oss << "\"created_at\":\"" << nowFormat << "\",";
    oss << "\"updated_at\":\"" << nowFormat << "\"";
    oss << "}";
    return oss.str();
}

void EsClient::indexDocument(int64_t noteId,
                             int64_t userId,
                             const std::string& title,
                             const std::string* content,
                             const std::string* summary,
                             const std::vector<std::string>& tags,
                             bool isPublic) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Put);
    req->setPath("/" + indexName_ + "/_doc/" + std::to_string(noteId));
    req->setContentTypeCode(CT_APPLICATION_JSON);
    
    std::string json = buildDocumentJson(userId, &title, content, summary, tags, isPublic);
    req->setBody(json);

    client_->sendRequest(req, [noteId](ReqResult result, const HttpResponsePtr& resp) {
        if (result != ReqResult::Ok) {
            LOG_ERROR << "ES index failed for note_id=" << noteId << ", result=" << static_cast<int>(result);
        } else if (resp && resp->getStatusCode() != 200 && resp->getStatusCode() != 201) {
            LOG_ERROR << "ES index failed for note_id=" << noteId << ", status=" << resp->getStatusCode();
            std::cout<<resp->getBody()<<std::endl;
        } else {
            LOG_DEBUG << "ES index success for note_id=" << noteId;
        }
    });
}

void EsClient::updateDocument(int64_t noteId,
                              const std::string* title,
                              const std::string* content,
                              const std::string* summary,
                              const std::vector<std::string>& tags,
                              const bool* isPublic) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Post);
    req->setPath("/" + indexName_ + "/_update/" + std::to_string(noteId));
    req->setContentTypeCode(CT_APPLICATION_JSON);

    std::ostringstream oss;
    oss << "{\"doc\":{";
    bool first = true;

    if (title) {
      if (!first) oss << ",";
      oss << "\"title\":\"" << escapeJson(title) << "\"";
      first = false;
    }
    if (content) {
      if (!first) oss << ",";
      oss << "\"content\":\"" << escapeJson(content) << "\"";
      first = false;
    }
    if (summary) {
      if (!first) oss << ",";
      oss << "\"summary\":\"" << escapeJson(summary) << "\"";
      first = false;
    }
    if (!tags.empty()) {
      if (!first) oss << ",";
      oss << "\"tags\":[";
      for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(&(tags[i])) << "\"";
      }
      oss << "]";
      first = false;
    }
    if (isPublic) {
      if (!first) oss << ",";
      oss << "\"is_public\":" << (*isPublic ? "true" : "false");
      first = false;
    }
    // 总是更新 updated_at
    if (!first) oss << ",";
    char timeBuf[32] = {0};
    trantor::Date::now().toCustomFormattedString("%Y-%m-%dT%H:%M:%S", timeBuf, sizeof(timeBuf));
    oss << "\"updated_at\":\"" << std::string(timeBuf) << "\"";

    oss << "}}";
    req->setBody(oss.str());

    client_->sendRequest(req, [noteId](ReqResult result, const HttpResponsePtr& resp) {
        if (result != ReqResult::Ok) {
            LOG_ERROR << "ES update failed for note_id=" << noteId << ", result=" << static_cast<int>(result);
        } else if (resp && resp->getStatusCode() != 200) {
            LOG_ERROR << "ES update failed for note_id=" << noteId << ", status=" << resp->getStatusCode();
        } else {
            LOG_DEBUG << "ES update success for note_id=" << noteId;
        }
    });
}

void EsClient::deleteDocument(int64_t noteId) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Delete);
    req->setPath("/" + indexName_ + "/_doc/" + std::to_string(noteId));

    client_->sendRequest(req, [noteId](ReqResult result, const HttpResponsePtr& resp) {
        if (result != ReqResult::Ok) {
            LOG_ERROR << "ES delete failed for note_id=" << noteId << ", result=" << static_cast<int>(result);
        } else if (resp && resp->getStatusCode() != 200 && resp->getStatusCode() != 404) {
            LOG_ERROR << "ES delete failed for note_id=" << noteId << ", status=" << resp->getStatusCode();
        } else {
            LOG_DEBUG << "ES delete success for note_id=" << noteId;
        }
    });
}

void EsClient::search(int64_t userId,
                      bool isPublic,
                      const std::string& keyword,
                      std::function<void(const std::vector<EsSearchResult>&)> callback,
                      int from,
                      int size) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Post);
    req->setPath("/" + indexName_ + "/_search");
    req->setContentTypeCode(CT_APPLICATION_JSON);

    // 构建过滤条件：
    // isPublic == true 时查询所有 is_public = true 且userId!=user_id的公开笔记
    // isPublic == false 时仅查询该 user_id 匹配的笔记
    std::string filterJson;
    if (isPublic == true) {
        // 公开笔记：必须 is_public=true + 排除当前登录用户自己的笔记
        filterJson = R"({
            "bool": {
                "must": [
                    { "term": { "is_public": true } }
                ],
                "must_not": [
                    { "term": { "user_id": )" + std::to_string(userId) + R"( } }
                ]
            }
        })";
    } else {
      // 个人笔记：只匹配自己的 user_id（不限制是否公开）
      filterJson = R"({ "term": { "user_id": )" + std::to_string(userId) + R"( } })";
    }

    std::string body = R"({
        "from": )" + std::to_string(from) + R"(,
        "size": )" + std::to_string(size) + R"(,
        "_source": ["title", "summary", "created_at", "updated_at"],  // 新增
        "query": {
            "bool": {
                "must": [
                    {
                        "multi_match": {
                            "query": ")" + escapeJson(&keyword) + R"(",
                            "fields": ["title^3", "content", "summary^2", "tags^2"],
                            "type": "best_fields",
                            "fuzziness": "AUTO"
                        }
                    }
                ],
                "filter": [
)" + filterJson + R"(
                ]
            }
        },
        "highlight": {
            "pre_tags": ["<mark>"],
            "post_tags": ["</mark>"],
            "fields": {
                "title": {"fragment_size": 100, "number_of_fragments": 1},
                "content": {"fragment_size": 200, "number_of_fragments": 3},
                "summary": {"fragment_size": 200, "number_of_fragments": 1}
            }
        },
        "sort": [
            {"_score": {"order": "desc"}}
        ]
    })";

    req->setBody(body);

    client_->sendRequest(req, [this, callback](ReqResult result, const HttpResponsePtr& resp) {
        if (result != ReqResult::Ok || !resp) {
            LOG_ERROR << "ES search failed, result=" << static_cast<int>(result);
            callback({});
            return;
        }

        if (resp->getStatusCode() != 200) {
            LOG_ERROR << "ES search failed, status=" << resp->getStatusCode() 
                      << ", body=" << std::string(resp->getBody());
            callback({});
            return;
        }

        std::string responseBody = std::string(resp->getBody());
        auto results = parseSearchResult(responseBody);
        callback(results);
    });
}

std::vector<EsSearchResult> EsClient::parseSearchResult(const std::string& jsonResponse) {
    std::vector<EsSearchResult> results;
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(jsonResponse.c_str(), jsonResponse.c_str() + jsonResponse.size(), &root, &errors)) {
        LOG_ERROR << "Failed to parse ES response: " << errors;
        return results;
    }

    if (!root.isMember("hits") || !root["hits"].isMember("hits")) {
        return results;
    }

    const Json::Value& hits = root["hits"]["hits"];
    for (const auto& hit : hits) {
        EsSearchResult result;
        
        // 获取ID
        if (hit.isMember("_id")) {
            result.noteId = std::stoll(hit["_id"].asString());
        }
        
        // 获取分数
        if (hit.isMember("_score")) {
            result.score = static_cast<float>(hit["_score"].asDouble());
        }

        // ===================== 新增：从 _source 提取字段 =====================
        if (hit.isMember("_source")) {
            const Json::Value& source = hit["_source"];
            
            if (source.isMember("title")) {
                result.title = source["title"].asString();
            }
            if (source.isMember("summary")) {
                result.summary = source["summary"].asString();
            }
            if (source.isMember("created_at")) {
                result.createdAt = source["created_at"].asString();
            }
            if (source.isMember("updated_at")) {
                result.updatedAt = source["updated_at"].asString();
            }
        }
        // ====================================================================

        // 获取高亮内容
        if (hit.isMember("highlight")) {
            const Json::Value& highlight = hit["highlight"];
            
            if (highlight.isMember("title") && highlight["title"].isArray() && highlight["title"].size() > 0) {
                result.highlightTitle = highlight["title"][0].asString();
            }
            
            if (highlight.isMember("content") && highlight["content"].isArray()) {
                std::ostringstream oss;
                for (const auto& frag : highlight["content"]) {
                    if (!oss.str().empty()) oss << " ... ";
                    oss << frag.asString();
                }
                result.highlightContent = oss.str();
            }
            
            // 如果没有content高亮，尝试使用summary
            if (result.highlightContent.empty() && 
                highlight.isMember("summary") && highlight["summary"].isArray() && highlight["summary"].size() > 0) {
                result.highlightContent = highlight["summary"][0].asString();
            }
        }

        results.push_back(result);
    }

    return results;
}

std::vector<EsSearchResult> EsClient::searchSync(int64_t userId,
                                                  bool isPublic,
                                                  const std::string& keyword,
                                                  int from,
                                                  int size,
                                                  int timeoutMs) {
    std::promise<std::vector<EsSearchResult>> promise;
    auto future = promise.get_future();

    search(userId, isPublic, keyword, [&promise](const std::vector<EsSearchResult>& results) {
        promise.set_value(results);
    }, from, size);

    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        LOG_ERROR << "ES search timeout";
        return {};
    }
    return future.get();
}

bool EsClient::ping(int timeoutMs) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Get);
    req->setPath("/");

    std::promise<bool> promise;
    auto future = promise.get_future();

    client_->sendRequest(req, [&promise](ReqResult result, const HttpResponsePtr& resp) {
        promise.set_value(result == ReqResult::Ok && resp && resp->getStatusCode() == 200);
    });

    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        return false;
    }
    return future.get();
}

bool EsClient::createIndex(int timeoutMs) {
    // 先检查索引是否存在
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Head);
    req->setPath("/" + indexName_);

    std::promise<bool> promise;
    auto future = promise.get_future();

    client_->sendRequest(req, [&promise](ReqResult result, const HttpResponsePtr& resp) {
        promise.set_value(result == ReqResult::Ok && resp && resp->getStatusCode() == 200);
    });

    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        LOG_ERROR << "ES check index timeout";
        return false;
    }

    bool exists = future.get();
    if (exists) {
        LOG_INFO << "ES index '" << indexName_ << "' already exists";
        return true;
    }

    // 创建索引
    auto createReq = HttpRequest::newHttpRequest();
    createReq->setMethod(Put);
    createReq->setPath("/" + indexName_);
    createReq->setContentTypeCode(CT_APPLICATION_JSON);

    // 定义映射
    std::string mapping = R"({
        "mappings": {
            "properties": {
                "title": {
                "type": "text",
                "analyzer": "ik_max_word"
              },
              "content": {
                "type": "text",
                "analyzer": "ik_max_word"
              },
              "summary": {
                "type": "text",
                "analyzer": "ik_max_word"
              },
              "tags": {
                "type": "keyword"
              },
              "user_id": {
                "type": "long"
              },
              "is_public": {
                "type": "boolean"
              },
              "created_at": {
                "type": "date"
              },
              "updated_at": {
                "type": "date"
              }
            }
        },
        "settings": {
            "number_of_shards": 1,
            "number_of_replicas": 0
        }
    })";

    createReq->setBody(mapping);

    std::promise<bool> createPromise;
    auto createFuture = createPromise.get_future();

    client_->sendRequest(createReq, [&createPromise](ReqResult result, const HttpResponsePtr& resp) {
        bool success = result == ReqResult::Ok && resp && 
                      (resp->getStatusCode() == 200 || resp->getStatusCode() == 201);
        createPromise.set_value(success);
    });

    if (createFuture.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        LOG_ERROR << "ES create index timeout";
        return false;
    }

    bool created = createFuture.get();
    if (created) {
        LOG_INFO << "ES index '" << indexName_ << "' created successfully";
    } else {
        LOG_ERROR << "ES index '" << indexName_ << "' creation failed";
    }
    return created;
}

} // namespace utils
} // namespace calcite
