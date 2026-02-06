#include "NoteTagController.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include "../models/Tag.h"

namespace calcite {
namespace api {
namespace v1 {

Json::Value NoteTagController::createResponse(int code, const std::string &message, const Json::Value &data) {
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

void NoteTagController::verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback) {
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

std::string insertMutiTagSql(std::string note_idStr, std::vector<int64_t> tagIds) {
  std::string insertSql = "INSERT INTO calcite.note_tag Values ";
  for (auto item : tagIds) {
    insertSql += '(' + note_idStr + ',' + std::to_string(item) + "),";
  }
  insertSql.pop_back();
  insertSql += ';';
  return insertSql;
}

void NoteTagController::bindTags(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
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

        int64_t noteId = json->get("note_id", 0).asInt64();
        if (noteId <= 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记ID无效"));
          callback(resp);
          return;
        }

        const Json::Value &tagIdsJson = json->get("tag_ids", Json::Value(Json::arrayValue));
        if (!tagIdsJson.isArray()) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "tag_ids必须是数组"));
          callback(resp);
          return;
        }

        std::vector<int64_t> tagIds;
        std::string          tagStr;
        for (const auto &tagId : tagIdsJson) {
          if (tagId.isInt64()) {
            tagIds.push_back(tagId.asInt64());
            tagStr += std::to_string(tagId.asInt64()) + ",";
          }
        }
        tagStr.pop_back(); // 移除最后一个逗号

        // 先删除该笔记的所有现有标签
        drogon::orm::Mapper<drogon_model::calcite::NoteTag> noteTagMapper(drogon::app().getDbClient("default"));
        noteTagMapper.deleteBy(drogon::orm::Criteria(drogon_model::calcite::NoteTag::Cols::_note_id, drogon::orm::CompareOperator::EQ, noteId), [this, noteId, tagIds, tagStr, userId, callback](size_t count)
            {
              // 如果tagIds为空，表示清除所有标签
              if (tagIds.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "清除标签成功"));
                callback(resp);
                return;
              }

              // 使用 SQL 查询检查所有标签是否存在且属于当前用户
              // SELECT user_id FROM calcite.tag WHERE user_id = ? AND id
              // IN (?, ?, ?, ...) HAVING COUNT(DISTINCT id) = ?
              std::string sql =
                  "SELECT user_id FROM calcite.tag WHERE user_id =" +
                  std::to_string(userId) + " AND id IN (" + tagStr +
                  ") HAVING COUNT(DISTINCT id) = " + std::to_string(tagIds.size()) +
                  ";";

              std::cout << "check tag sql:" << sql << std::endl;
              auto client = drogon::app().getDbClient("default");

              client->execSqlAsync(
                  sql, [this, noteId, tagIds, callback](const drogon::orm::Result &result)
                  {
                    // 如果查询结果为空，说明部分标签不存在或无权访问
                    if (result.size() == 0) {
                      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "部分标签不存在或无权访问"));
                      callback(resp);
                      return;
                    }

                    std::cout << "tag check passed, inserting note tags"
                              << std::endl;

                    // insert into not_tag values (1,2),(1,3),(1,4);
                    std::string insertSQL = insertMutiTagSql(std::to_string(noteId), tagIds);
                    std::cout << insertSQL << '\n';

                    auto clientToInsert = drogon::app().getDbClient("default");
                    clientToInsert->execSqlAsync(
                        insertSQL,
                        [this, callback](const drogon::orm::Result &result)
                        {
                          auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "绑定标签成功"));
                          callback(resp);
                          return;
                        },
                        [this, callback](const drogon::orm::DrogonDbException &e)
                        {
                          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "绑定标签失败: " + std::string(e.base().what())));
                          callback(resp);
                        });
                  },
                  [callback, this](const drogon::orm::DrogonDbException &e)
                  {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "检查标签失败: " + std::string(e.base().what())));
                    callback(resp);
                  });
            },
            [callback, this](const drogon::orm::DrogonDbException &e)
            {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "删除现有标签失败: " + std::string(e.base().what())));
              callback(resp);
            });
      });
}

} // namespace v1
} // namespace api
} // namespace calcite
