#include "EsClient.h"
#include <drogon/HttpRequest.h>
#include <future>
#include <chrono>

using namespace drogon;

void EsClient::indexDocument(const std::string& index,
                             const std::string& id,
                             const std::string& json)
{
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Put);
    req->setPath("/" + index + "/_doc/" + id);
    req->setContentTypeCode(CT_APPLICATION_JSON);
    req->setBody(json);

    client_->sendRequest(req, [](ReqResult result, const HttpResponsePtr& resp) {
        if (result != ReqResult::Ok) {
            std::cout << "ES index failed" << std::endl;
            LOG_ERROR << "ES index failed";
        }
    });
}

bool EsClient::indexDocumentSync(const std::string& index,
                                   const std::string& id,
                                   const std::string& json,
                                   int timeoutMs)
{
    std::promise<bool> done;
    std::future<bool> future = done.get_future();

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Put);
    req->setPath("/" + index + "/_doc/" + id);
    req->setContentTypeCode(CT_APPLICATION_JSON);
    req->setBody(json);

    client_->sendRequest(req, [&done](ReqResult result, const HttpResponsePtr& resp) {
        bool success = (result == ReqResult::Ok && resp);
        if (!success) {
            LOG_ERROR << "ES index failed: " << static_cast<int>(result);
        }
        done.set_value(success);
    });

    // 等待结果，带超时
    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        LOG_ERROR << "ES index timeout";
        return false;
    }
    return future.get();
}

void EsClient::deleteDocument(const std::string& index,
                              const std::string& id)
{
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Delete);
    req->setPath("/" + index + "/_doc/" + id);

    client_->sendRequest(req, [](ReqResult result, const HttpResponsePtr& resp) {
        if (result != ReqResult::Ok) {
            LOG_ERROR << "ES delete failed";
        }
    });
}

bool EsClient::deleteDocumentSync(const std::string& index,
                                   const std::string& id,
                                   int timeoutMs)
{
    std::promise<bool> done;
    std::future<bool> future = done.get_future();

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Delete);
    req->setPath("/" + index + "/_doc/" + id);

    client_->sendRequest(req, [&done](ReqResult result, const HttpResponsePtr& resp) {
        bool success = (result == ReqResult::Ok && resp);
        if (!success) {
            LOG_ERROR << "ES delete failed: " << static_cast<int>(result);
        }
        done.set_value(success);
    });

    // 等待结果，带超时
    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        LOG_ERROR << "ES delete timeout";
        return false;
    }
    return future.get();
}

void EsClient::search(const std::string& index,
                      const std::string& query,
                      std::function<void(const std::string&)> callback)
{
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Post);
    req->setPath("/" + index + "/_search");
    req->setContentTypeCode(CT_APPLICATION_JSON);

    std::string body = R"({
        "query": {
            "multi_match": {
                "query": ")" + query + R"(",
                "fields": ["title", "content"]
            }
        }
    })";

    req->setBody(body);

    client_->sendRequest(req, [callback](ReqResult result, const HttpResponsePtr& resp) {
        if (result == ReqResult::Ok) {
            callback(std::string(resp->getBody()));
        } else {
            callback("");
        }
    });
}

std::string EsClient::searchSync(const std::string& index,
                                  const std::string& query,
                                  int timeoutMs)
{
    std::promise<std::string> done;
    std::future<std::string> future = done.get_future();

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Post);
    req->setPath("/" + index + "/_search");
    req->setContentTypeCode(CT_APPLICATION_JSON);

    std::string body = R"({
        "query": {
            "multi_match": {
                "query": ")" + query + R"(",
                "fields": ["title", "content"]
            }
        }
    })";

    req->setBody(body);

    client_->sendRequest(req, [&done](ReqResult result, const HttpResponsePtr& resp) {
        if (result == ReqResult::Ok && resp) {
            done.set_value(std::string(resp->getBody()));
        } else {
            LOG_ERROR << "ES search failed: " << static_cast<int>(result);
            done.set_value("");
        }
    });

    // 等待结果，带超时
    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        LOG_ERROR << "ES search timeout";
        return "";
    }
    return future.get();
}

drogon::HttpClientPtr EsClient::getClient() const {
    return client_;
}
