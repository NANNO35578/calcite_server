#pragma once

#include "../models/User.h"
#include "../services/AuthService.h"
#include <drogon/HttpController.h>
#include <drogon/orm/Mapper.h>

using namespace drogon;

namespace calcite {
namespace api {
namespace v1 {

class UserController : public drogon::HttpController<UserController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(UserController::getProfile, "/api/user/profile", Get);
  METHOD_LIST_END

  void getProfile(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

private:
  services::AuthService authService_;
  Json::Value           createResponse(int code, const std::string &message, const Json::Value &data = Json::Value());
};

} // namespace v1
} // namespace api
} // namespace calcite
