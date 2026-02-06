#pragma once

#include "../services/AuthService.h"
#include <drogon/HttpController.h>

using namespace drogon;

namespace calcite {
namespace api {
namespace v1 {

class AuthController : public drogon::HttpController<AuthController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO (AuthController::registerUser, "/api/auth/register", Post);
  ADD_METHOD_TO (AuthController::loginUser, "/api/auth/login", Post);
  ADD_METHOD_TO (AuthController::logoutUser, "/api/auth/logout", Post);
  METHOD_LIST_END

  void registerUser (const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);
  void loginUser (const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);
  void logoutUser (const HttpRequestPtr &req, std::function<void (const HttpResponsePtr &)> &&callback);

private:
  services::AuthService authService_;
  Json::Value createResponse (int code, const std::string &message, const Json::Value &data = Json::Value ());
};

} // namespace v1
} // namespace api
} // namespace calcite
