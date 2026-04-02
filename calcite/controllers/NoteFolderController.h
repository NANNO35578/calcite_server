#pragma once

#include "../services/AuthService.h"
#include "../services/NoteFolderService.h"
#include <drogon/HttpController.h>

using namespace drogon;

namespace calcite {
namespace api {
namespace v1 {

class NoteFolderController : public drogon::HttpController<NoteFolderController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(NoteFolderController::createFolder, "/api/folder/create", Post);
  ADD_METHOD_TO(NoteFolderController::listFolders, "/api/folder/list", Get);
  ADD_METHOD_TO(NoteFolderController::updateFolder, "/api/folder/update", Post);
  ADD_METHOD_TO(NoteFolderController::deleteFolder, "/api/folder/delete", Post);
  METHOD_LIST_END

  void createFolder(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void listFolders(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void updateFolder(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void deleteFolder(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

private:
  services::AuthService authService_;
  services::NoteFolderService folderService_;

  Json::Value createResponse(int code, const std::string &message, const Json::Value &data = Json::Value());
  bool verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback);
};

} // namespace v1
} // namespace api
} // namespace calcite
