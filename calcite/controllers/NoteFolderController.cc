#include "NoteFolderController.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace calcite {
namespace api {
namespace v1 {

Json::Value NoteFolderController::createResponse(int code, const std::string &message, const Json::Value &data) {
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

bool NoteFolderController::verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback) {
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

  authService_.verifyToken(token, [callback](bool valid, int64_t userId, const std::string &)
      {
        callback(valid, userId);
      });
  return true;
}

void NoteFolderController::createFolder(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
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
        int64_t parentId = json->get("parent_id", 0).asInt64();

        folderService_.createFolder(userId, name, parentId,
            [callback, this](const services::CreateFolderResult &result)
            {
              Json::Value data;
              data["folder_id"] = static_cast<Json::Int64>(result.folderId);
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message, data));
              callback(resp);
            });
      });
}

void NoteFolderController::listFolders(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
        if (!valid) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
          callback(resp);
          return;
        }

        std::string folderIdStr = req->getParameter("folder_id");
        int64_t parentId = folderIdStr.empty() ? 0 : std::stoll(folderIdStr);

        folderService_.listFolders(userId, parentId,
            [callback, this](bool success, const std::vector<services::FolderDetail> &folders, const std::string &message)
            {
              if (!success) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, message));
                callback(resp);
                return;
              }

              Json::Value data(Json::arrayValue);
              for (const auto &folder : folders) {
                Json::Value folderJson;
                folderJson["id"] = static_cast<Json::Int64>(folder.id);
                folderJson["name"] = folder.name;
                folderJson["parent_id"] = static_cast<Json::Int64>(folder.parentId);
                folderJson["created_at"] = folder.createdAt;
                data.append(folderJson);
              }

              auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取文件夹列表成功", data));
              callback(resp);
            });
      });
}

void NoteFolderController::updateFolder(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
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

        int64_t folderId = json->get("folder_id", 0).asInt64();
        if (folderId <= 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "文件夹ID无效"));
          callback(resp);
          return;
        }

        std::string name = json->get("name", "").asString();
        int64_t parentId = json->get("parent_id", -1).asInt64();

        if (name.empty() && parentId < 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "请至少提供 name 或 parent_id 参数"));
          callback(resp);
          return;
        }

        if (!name.empty()) {
          folderService_.updateFolder(userId, folderId, name, parentId,
              [callback, this](const services::UpdateFolderResult &result)
              {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message));
                callback(resp);
              });
        } else {
          folderService_.updateFolder(userId, folderId, "", parentId,
              [callback, this](const services::UpdateFolderResult &result)
              {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message));
                callback(resp);
              });
        }
      });
}

void NoteFolderController::deleteFolder(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
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

        int64_t folderId = json->get("folder_id", 0).asInt64();
        if (folderId <= 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "文件夹ID无效"));
          callback(resp);
          return;
        }

        folderService_.deleteFolder(userId, folderId,
            [callback, this](const services::DeleteFolderResult &result)
            {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message));
              callback(resp);
            });
      });
}

} // namespace v1
} // namespace api
} // namespace calcite
