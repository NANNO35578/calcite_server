#include "../models/Note.h"
#include "../models/NoteTag.h"
#include "TagController.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace calcite {
namespace api {
namespace v1 {

Json::Value TagController::createResponse(int code, const std::string &message, const Json::Value &data) {
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

void TagController::verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback) {
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

void TagController::createTag(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
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
        if (name.empty()) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "标签名称不能为空"));
          callback(resp);
          return;
        }

        // 创建标签
        drogon_model::calcite::Tag tag;
        tag.setUserId(userId);
        tag.setName(name);

        drogon::orm::Mapper<drogon_model::calcite::Tag> tagMapper(drogon::app().getDbClient("default"));
        tagMapper.insert(
            tag,
            [callback, this](const drogon_model::calcite::Tag &insertedTag)
            {
              Json::Value data;
              data["tag_id"] = static_cast<Json::Int64>(insertedTag.getValueOfId());
              auto resp      = HttpResponse::newHttpJsonResponse(createResponse(0, "创建标签成功", data));
              callback(resp);
            },
            [callback, this](const drogon::orm::DrogonDbException &e)
            {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "创建标签失败: " + std::string(e.base().what())));
              callback(resp);
            });
      });
}

void TagController::listTags(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
        if (!valid) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
          callback(resp);
          return;
        }

        std::string noteIdStr = req->getParameter("note_id");
        auto        dbClient  = drogon::app().getDbClient("default");

        // 如果提供了 note_id，查询该笔记关联的标签
        if (!noteIdStr.empty()) {
          int64_t noteId;
          try {
            noteId = std::stoll(noteIdStr);
            if (noteId <= 0) {
              throw std::invalid_argument("Invalid note_id");
            }
          } catch (...) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "note_id参数无效"));
            callback(resp);
            return;
          }

          // 先验证该笔记是否属于当前用户且未删除
          drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(dbClient);
          noteMapper.findByPrimaryKey(
              noteId,
              [this, noteId, userId, callback, dbClient](const drogon_model::calcite::Note &note)
              {
                if (note.getValueOfUserId() != userId || note.getValueOfIsDeleted() != 0) {
                  auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记ID无效或无权访问"));
                  callback(resp);
                  return;
                }

                // 查询 note_tag 表找到关联的 tag_id
                drogon::orm::Mapper<drogon_model::calcite::NoteTag> noteTagMapper(dbClient);
                noteTagMapper.findBy(
                    drogon::orm::Criteria(drogon_model::calcite::NoteTag::Cols::_note_id, drogon::orm::CompareOperator::EQ, noteId),
                    [this, userId, callback, dbClient](const std::vector<drogon_model::calcite::NoteTag> &noteTags)
                    {
                      if (noteTags.empty()) {
                        auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取标签列表成功", Json::Value(Json::arrayValue)));
                        callback(resp);
                        return;
                      }

                      // 提取 tag_id 列表
                      std::vector<int64_t> tagIds;
                      for (const auto &nt : noteTags) {
                        tagIds.push_back(nt.getValueOfTagId());
                      }

                      // 查询这些标签的详细信息
                      drogon::orm::Mapper<drogon_model::calcite::Tag> tagMapper(dbClient);
                      auto                                            criteria = drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId);
                      criteria                                                 = criteria && drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_id, drogon::orm::CompareOperator::In, tagIds);

                      tagMapper.findBy(
                          criteria,
                          [callback, this](const std::vector<drogon_model::calcite::Tag> &tags)
                          {
                            Json::Value data(Json::arrayValue);
                            for (const auto &tag : tags) {
                              Json::Value tagJson;
                              tagJson["id"]   = static_cast<Json::Int64>(tag.getValueOfId());
                              tagJson["name"] = tag.getValueOfName();
                              if (tag.getCreatedAt()) {
                                tagJson["created_at"] = tag.getCreatedAt()->toDbStringLocal();
                              }
                              data.append(tagJson);
                            }

                            auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取标签列表成功", data));
                            callback(resp);
                          },
                          [callback, this](const drogon::orm::DrogonDbException &e)
                          {
                            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取标签列表失败: " + std::string(e.base().what())));
                            callback(resp);
                          });
                    },
                    [callback, this](const drogon::orm::DrogonDbException &e)
                    {
                      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取标签列表失败: " + std::string(e.base().what())));
                      callback(resp);
                    });
              },
              [callback, this](const drogon::orm::DrogonDbException &e)
              {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记ID无效"));
                callback(resp);
              });
        } else {
          // 没有提供 note_id，查询用户的所有标签
          drogon::orm::Mapper<drogon_model::calcite::Tag> tagMapper(dbClient);
          tagMapper.findBy(
              drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId),
              [callback, this](const std::vector<drogon_model::calcite::Tag> &tags)
              {
                Json::Value data(Json::arrayValue);
                for (const auto &tag : tags) {
                  Json::Value tagJson;
                  tagJson["id"]   = static_cast<Json::Int64>(tag.getValueOfId());
                  tagJson["name"] = tag.getValueOfName();
                  if (tag.getCreatedAt()) {
                    tagJson["created_at"] = tag.getCreatedAt()->toDbStringLocal();
                  }
                  data.append(tagJson);
                }

                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取标签列表成功", data));
                callback(resp);
              },
              [callback, this](const drogon::orm::DrogonDbException &e)
              {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取标签列表失败: " + std::string(e.base().what())));
                callback(resp);
              });
        }
      });
}

} // namespace v1
} // namespace api
} // namespace calcite
