#include "UserController.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace calcite {
namespace api {
namespace v1 {

Json::Value UserController::createResponse(int code, const std::string& message, const Json::Value& data) {
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

void UserController::getProfile(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    // 从 header 中获取 token
    std::string token = req->getHeader("Authorization");
    if (!token.empty() && token.find("Bearer ") == 0) {
        token = token.substr(7);
    }
    
    // 如果 header 中没有，尝试从 query 参数获取
    if (token.empty()) {
        token = req->getParameter("token");
    }
    
    if (token.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "未提供认证token"));
        callback(resp);
        return;
    }
    
    // 验证 token 并获取用户信息
    authService_.verifyToken(token, [callback, this](bool valid, int64_t userId, const std::string& username) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }
        
        // 查询用户详细信息
        auto dbClient = drogon::app().getDbClient("default");
        std::string sql = "SELECT id, username, email, avatar, created_at, updated_at FROM user WHERE id = ?";
        dbClient->execSqlAsync(
            sql,
            [callback, this](const drogon::orm::Result& r) {
                if (r.empty()) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "用户不存在"));
                    callback(resp);
                    return;
                }
                
                Json::Value data;
                data["id"] = static_cast<Json::Int64>(r[0]["id"].as<int64_t>());
                data["username"] = r[0]["username"].as<std::string>();
                if (!r[0]["email"].isNull()) {
                    data["email"] = r[0]["email"].as<std::string>();
                }
                if (!r[0]["avatar"].isNull()) {
                    data["avatar"] = r[0]["avatar"].as<std::string>();
                }
                data["created_at"] = r[0]["created_at"].as<std::string>();
                data["updated_at"] = r[0]["updated_at"].as<std::string>();
                
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "success", data));
                callback(resp);
            },
            [callback, this](const drogon::orm::DrogonDbException& e) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "查询用户信息失败: " + std::string(e.base().what())));
                callback(resp);
            },
            userId);
    });
}

} // namespace v1
} // namespace api
} // namespace calcite

