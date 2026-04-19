// #pragma once

// #include "../models/NoteTag.h"
// #include "../services/AuthService.h"
// #include <drogon/HttpController.h>

// using namespace drogon;

// namespace calcite {
// namespace api {
// namespace v1 {

// class NoteTagController : public drogon::HttpController<NoteTagController> {
// public:
//   METHOD_LIST_BEGIN
//   ADD_METHOD_TO(NoteTagController::bindTags, "/api/tag/bind", Post);
//   METHOD_LIST_END

//   void bindTags(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

// private:
//   services::AuthService authService_;

//   Json::Value createResponse(int code, const std::string &message, const Json::Value &data = Json::Value());
//   void        verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback);
// };

// } // namespace v1
// } // namespace api
// } // namespace calcite
