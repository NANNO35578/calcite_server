#include "TagController.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace calcite {
namespace api {
namespace v1 {

Json::Value TagController::createResponse(int code, const std::string &message, const Json::Value &data) {
  Json::Value response;
  response["code"]    = code;
  response["message"] = message;
  if (!data.isNull()) {
    response["data"] = data;
  } else {
    response["data"] = Json::Value(Json::objectValue);
  }
  return response;
}

void TagController::verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback) {
  std::string token = req->getHeader("Authorization");
  if (!token.empty() && token.find("Bearer ") == 0) {
    token = token.substr(7);
  }
  if (token.empty()) {
    token = req->getParameter("token");
  }
  if (token.empty()) {
    callback(false, 0);
    return;
  }

  authService_.verifyToken(token, [callback](bool valid, int64_t userId, const std::string &)
      {
        callback(valid, userId);
      });
}

void TagController::getHotTags(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
        if (!valid) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
          callback(resp);
          return;
        }

        auto client = drogon::HttpClient::newHttpClient("http://localhost:9200");
        auto esReq = drogon::HttpRequest::newHttpRequest();
        esReq->setMethod(drogon::Post);
        esReq->setPath("/notes/_search");
        esReq->setContentTypeCode(drogon::CT_APPLICATION_JSON);

        std::string body = R"({
  "size": 0,
  "query": {
    "bool": {
      "filter": [
        { "term": { "is_public": true }},
        {
          "range": {
            "created_at": {
              "gte": "now-7d/d"
            }
          }
        }
      ]
    }
  },
  "aggs": {
    "hot_tags": {
      "terms": {
        "field": "tags",
        "size": 10
      }
    }
  }
})";

        esReq->setBody(body);

        client->sendRequest(esReq, [this, callback](drogon::ReqResult result, const drogon::HttpResponsePtr &resp)
            {
              if (result != drogon::ReqResult::Ok || !resp) {
                auto r = HttpResponse::newHttpJsonResponse(createResponse(1, "ES查询失败"));
                callback(r);
                return;
              }

              if (resp->getStatusCode() != 200) {
                auto r = HttpResponse::newHttpJsonResponse(createResponse(1, "ES查询失败，状态码: " + std::to_string(resp->getStatusCode())));
                callback(r);
                return;
              }

              std::string responseBody = std::string(resp->getBody());
              Json::Value root;
              Json::CharReaderBuilder builder;
              std::string errors;
              std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
              if (!reader->parse(responseBody.c_str(), responseBody.c_str() + responseBody.size(), &root, &errors)) {
                auto r = HttpResponse::newHttpJsonResponse(createResponse(1, "解析ES响应失败"));
                callback(r);
                return;
              }

              Json::Value data(Json::arrayValue);
              if (root.isMember("aggregations") && root["aggregations"].isMember("hot_tags") &&
                  root["aggregations"]["hot_tags"].isMember("buckets")) {
                const Json::Value &buckets = root["aggregations"]["hot_tags"]["buckets"];
                for (const auto &bucket : buckets) {
                  Json::Value item;
                  item["tag"] = bucket["key"].asString();
                  item["count"] = static_cast<Json::Int64>(bucket["doc_count"].asInt64());
                  data.append(item);
                }
              }

              auto r = HttpResponse::newHttpJsonResponse(createResponse(0, "获取热门标签成功", data));
              callback(r);
            });
      });
}

} // namespace v1
} // namespace api
} // namespace calcite
