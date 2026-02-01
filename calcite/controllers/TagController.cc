#include "TagController.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace calcite {
namespace api {
namespace v1 {

Json::Value TagController::createResponse(int code, const std::string& message, const Json::Value& data) {
    Json::Value response;
    response["code"] = code;
    response["message"] = message;
    if (!data.isNull()) {
        response["data"] = data;
    } else {
        response["data"] = Json::Value(Json::objectValue);
    }
    return response;
}

bool TagController::verifyTokenAndGetUserId(const HttpRequestPtr& req, std::function<void(bool, int64_t)> callback) {
    std::string token = req->getHeader("Authorization");
    if (!token.empty() && token.find("Bearer ") == 0) {
        token = token.substr(7);
    }
    if (token.empty()) {
        token = req->getParameter("token");
    }
    if (token.empty()) {
        callback(false, 0);
        return false;
    }

    authService_.verifyToken(token, [callback](bool valid, int64_t userId, const std::string&) {
        callback(valid, userId);
    });
    return true;
}

void TagController::createTag(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "请求参数错误"));
            callback(resp);
            return;
        }

        std::string name = json->get("name", "").asString();

        tagService_.createTag(userId, name,
            [callback, this](const services::CreateTagResult& result) {
                Json::Value data;
                data["tag_id"] = static_cast<Json::Int64>(result.tagId);
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message, data));
                callback(resp);
            });
    });
}

void TagController::listTags(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    verifyTokenAndGetUserId(req, [this, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        tagService_.listTags(userId,
            [callback, this](bool success, const std::vector<services::TagDetail>& tags, const std::string& message) {
                if (!success) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, message));
                    callback(resp);
                    return;
                }

                Json::Value data(Json::arrayValue);
                for (const auto& tag : tags) {
                    Json::Value tagJson;
                    tagJson["id"] = static_cast<Json::Int64>(tag.id);
                    tagJson["name"] = tag.name;
                    tagJson["created_at"] = tag.createdAt;
                    data.append(tagJson);
                }

                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取标签列表成功", data));
                callback(resp);
            });
    });
}

void TagController::bindTagsToNote(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "请求参数错误"));
            callback(resp);
            return;
        }

        if (!json->isMember("note_id")) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "缺少note_id参数"));
            callback(resp);
            return;
        }

        int64_t noteId = (*json)["note_id"].asInt64();
        std::vector<int64_t> tagIds;

        if (json->isMember("tag_ids") && (*json)["tag_ids"].isArray()) {
            for (const auto& item : (*json)["tag_ids"]) {
                tagIds.push_back(item.asInt64());
            }
        }

        tagService_.bindTagsToNote(noteId, userId, tagIds,
            [callback, this](const services::BindTagResult& result) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message));
                callback(resp);
            });
    });
}

} // namespace v1
} // namespace api
} // namespace calcite
