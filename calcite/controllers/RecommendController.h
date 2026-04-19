#pragma once

#include "../services/AuthService.h"
#include "../utils/EsClient.h"
#include <drogon/HttpController.h>

using namespace drogon;

namespace calcite {
namespace api {
namespace v1 {

class RecommendController : public drogon::HttpController<RecommendController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(RecommendController::recommendNotes, "/api/recommend/notes", Get);
  METHOD_LIST_END

  void recommendNotes(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

private:
  services::AuthService authService_;
  utils::EsClient       esClient_;

  Json::Value createResponse(int code, const std::string &message, const Json::Value &data = Json::Value());
  void        verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback);

  void fetchNewUserTags(int64_t userId, std::function<void(const std::vector<std::string>&)> callback);
  void fetchOldUserTags(int64_t userId, std::function<void(const std::vector<std::string>&)> callback);
};

} // namespace v1
} // namespace api
} // namespace calcite
