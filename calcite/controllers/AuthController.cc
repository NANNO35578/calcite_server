#include "AuthController.h"
#include <drogon/drogon.h>
#include <iostream>
#include <json/json.h>

namespace calcite {
namespace api {
namespace v1 {

Json::Value AuthController::createResponse(int code, const std::string &message,
                                           const Json::Value &data) {
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

void AuthController::registerUser(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json) {
    auto resp =
        HttpResponse::newHttpJsonResponse(createResponse(1, "请求参数错误"));
    callback(resp);
    return;
  }

  std::string username = json->get("username", "").asString();
  std::string email = json->get("email", "").asString();
  std::string password = json->get("password", "").asString();

  if (username.empty() || password.empty()) {
    auto resp = HttpResponse::newHttpJsonResponse(
        createResponse(1, "用户名和密码不能为空"));
    callback(resp);
    return;
  }

  authService_.registerUser(
      username, email, password,
      [callback, this](const services::RegisterResult &result) {
        if (result.success) {
          Json::Value data;
          data["user_id"] = static_cast<Json::Int64>(result.userId);
          auto resp = HttpResponse::newHttpJsonResponse(
              createResponse(0, result.message, data));
          callback(resp);
        } else {
          auto resp = HttpResponse::newHttpJsonResponse(
              createResponse(1, result.message));
          callback(resp);
        }
      });
}

void AuthController::loginUser(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json) {
    auto resp =
        HttpResponse::newHttpJsonResponse(createResponse(1, "请求参数错误"));
    callback(resp);
    return;
  }

  std::string username = json->get("username", "").asString();
  std::string password = json->get("password", "").asString();

  if (username.empty() || password.empty()) {
    auto resp = HttpResponse::newHttpJsonResponse(
        createResponse(1, "用户名和密码不能为空"));
    callback(resp);
    return;
  }

  authService_.loginUser(username, password,
                         [callback, this](const services::LoginResult &result) {
                           if (result.success) {
                             Json::Value data;
                             data["token"] = result.token;
                             data["user_id"] =
                                 static_cast<Json::Int64>(result.userId);
                             data["username"] = result.username;
                             auto resp = HttpResponse::newHttpJsonResponse(
                                 createResponse(0, result.message, data));
                             callback(resp);
                           } else {
                             auto resp = HttpResponse::newHttpJsonResponse(
                                 createResponse(1, result.message));
                             callback(resp);
                           }
                         });
}

void AuthController::logoutUser(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json) {
    auto resp =
        HttpResponse::newHttpJsonResponse(createResponse(1, "请求参数错误"));
    callback(resp);
    return;
  }

  std::string token = json->get("token", "").asString();
  if (token.empty()) {
    // 尝试从 header 中获取
    token = req->getHeader("Authorization");
    if (!token.empty() && token.find("Bearer ") == 0) {
      token = token.substr(7);
    }
  }

  if (token.empty()) {
    auto resp =
        HttpResponse::newHttpJsonResponse(createResponse(1, "Token不能为空"));
    callback(resp);
    return;
  }

  authService_.logoutUser(token, [callback, this](bool success,
                                                  const std::string &message) {
    if (success) {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, message));
      callback(resp);
    } else {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, message));
      callback(resp);
    }
  });
}

} // namespace v1
} // namespace api
} // namespace calcite
