#include "NoteController.h"
#include <algorithm>
#include <drogon/drogon.h>
#include <json/json.h>
#include <sstream>

namespace calcite {
namespace api {
namespace v1 {

Json::Value NoteController::createResponse(int code, const std::string &message, const Json::Value &data) {
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

void NoteController::verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback) {
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

void NoteController::getNoteTags(int64_t noteId, std::function<void(std::vector<std::string>)> callback) {
  auto dbClient = drogon::app().getDbClient("default");
  std::string sql = 
    "SELECT t.name FROM tag t "
    "INNER JOIN note_tag nt ON t.id = nt.tag_id "
    "WHERE nt.note_id = ?";
  
  dbClient->execSqlAsync(
    sql,
    [callback](const drogon::orm::Result &result) {
      std::vector<std::string> tags;
      for (const auto &row : result) {
        tags.push_back(row["name"].as<std::string>());
      }
      callback(tags);
    },
    [callback](const drogon::orm::DrogonDbException &e) {
      LOG_ERROR << "Failed to get note tags: " << e.base().what();
      callback({});
    },
    noteId
  );
}

void NoteController::indexNoteToES(int64_t noteId, int64_t userId, const drogon_model::calcite::Note& note,
                                   const std::vector<std::string>& tags) {
  esClient_.indexDocument(
    noteId,
    userId,
    note.getValueOfTitle(),
    note.getValueOfContent(),
    note.getValueOfSummary(),
    tags
  );
}

void NoteController::createNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
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

        std::string title    = json->get("title", "").asString();
        std::string content  = json->get("content", "").asString();
        std::string summary  = json->get("summary", "").asString();
        int64_t     folderId = json->get("folder_id", 0).asInt64();

        if (title.empty()) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "标题和内容不能为空"));
          callback(resp);
          return;
        }

        // 创建笔记
        drogon_model::calcite::Note note;
        note.setUserId(userId);
        note.setTitle(title);
        note.setContent(content);
        note.setSummary(summary);
        note.setFolderId(folderId);
        
        drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(drogon::app().getDbClient("default"));
        noteMapper.insert(
            note,
            [this, callback, userId](const drogon_model::calcite::Note &insertedNote)
            {
              int64_t noteId = insertedNote.getValueOfId();
              
              // 异步索引到ES（空标签列表，因为新笔记还没有标签）默认不公开
              indexNoteToES(noteId, userId, insertedNote, {});
              
              Json::Value data;
              data["note_id"] = static_cast<Json::Int64>(noteId);
              auto resp       = HttpResponse::newHttpJsonResponse(createResponse(0, "创建笔记成功", data));
              callback(resp);
            },
            [callback, this](const drogon::orm::DrogonDbException &e)
            {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "创建笔记失败: " + std::string(e.base().what())));
              callback(resp);
            });
      });
}

void NoteController::updateNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
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

        std::string title    = json->get("title", "").asString();
        std::string content  = json->get("content", "").asString();
        std::string summary  = json->get("summary", "").asString();
        int64_t     folderId = json->get("folder_id", 0).asInt64();
        bool        isPublic = json->get("is_public", false).asBool();

        // 检查笔记是否存在且属于当前用户
        drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(drogon::app().getDbClient("default"));
        noteMapper.findByPrimaryKey(
            noteId,
            [this, userId, noteId, title, content, summary, folderId, isPublic, callback, &noteMapper](const drogon_model::calcite::Note &note)
            {
              if (note.getValueOfUserId() != userId) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问该笔记"));
                callback(resp);
                return;
              }

              // 更新笔记
              drogon_model::calcite::Note updatedNote = note;
              if (!title.empty()) updatedNote.setTitle(title);
              if (!content.empty()) updatedNote.setContent(content);
              if (!summary.empty()) updatedNote.setSummary(summary);
              if (folderId >= 0) updatedNote.setFolderId(folderId);
              if (isPublic) updatedNote.setIsPublic(isPublic);

              noteMapper.update(
                  updatedNote,
                  [this, userId, noteId, title, content, summary, isPublic, callback, &note](size_t count)
                  {
                    // 异步更新ES
                    const std::string* pTitle = title.empty() ? nullptr : &title;
                    const std::string* pContent = content.empty() ? nullptr : &content;
                    const std::string* pSummary = summary.empty() ? nullptr : &summary;
                    const bool* pIsPublic = isPublic ? &isPublic : nullptr;
                    
                    esClient_.updateDocument(noteId, pTitle, pContent, pSummary, nullptr, pIsPublic);
                    
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "更新笔记成功"));
                    callback(resp);
                  },
                  [callback, this](const drogon::orm::DrogonDbException &e)
                  {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "更新笔记失败: " + std::string(e.base().what())));
                    callback(resp);
                  });
            },
            [callback, this](const drogon::orm::DrogonDbException &e)
            {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "查询笔记失败: " + std::string(e.base().what())));
              callback(resp);
            });
      });
}

void NoteController::deleteNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
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

        // 检查笔记是否存在且属于当前用户
        drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(drogon::app().getDbClient("default"));
        noteMapper.findByPrimaryKey(
            noteId,
            [this, userId, noteId, callback, &noteMapper](const drogon_model::calcite::Note &note)
            {
              if (note.getValueOfUserId() != userId) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问该笔记"));
                callback(resp);
                return;
              }

              // 标记为已删除
              drogon_model::calcite::Note deletedNote = note;
              deletedNote.setIsDeleted(true);
              noteMapper.update(
                  deletedNote,
                  [this, noteId, callback](size_t count)
                  {
                    // 异步从ES删除
                    esClient_.deleteDocument(noteId);
                    
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "删除笔记成功"));
                    callback(resp);
                  },
                  [callback, this](const drogon::orm::DrogonDbException &e)
                  {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "删除笔记失败: " + std::string(e.base().what())));
                    callback(resp);
                  });
            },
            [callback, this](const drogon::orm::DrogonDbException &e)
            {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "查询笔记失败: " + std::string(e.base().what())));
              callback(resp);
            });
      });
}

void NoteController::listNotes(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
        if (!valid) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
          callback(resp);
          return;
        }

        std::string folderIdStr = req->getParameter("folder_id");
        int64_t     folderId    = folderIdStr.empty() ? 0 : std::stoll(folderIdStr);

        std::string          tagIdsStr = req->getParameter("tag_ids");
        std::vector<int64_t> tagIds;
        if (!tagIdsStr.empty()) {
          std::stringstream ss(tagIdsStr);
          std::string       item;
          while (std::getline(ss, item, ',')) {
            try {
              if (!item.empty())
                tagIds.push_back(std::stoll(item));
            } catch (...) {
              // 忽略无效的tag_id
            }
          }
        }

        auto dbClient = drogon::app().getDbClient("default");

        // 构建基础查询条件
        std::string whereClause    = "WHERE user_id = ? AND is_deleted = 0";
        bool        hasFolderParam = false;

        // 添加 folder_id 过滤
        if (folderId >= 0) {
          whereClause += " AND folder_id = ?";
          hasFolderParam = true;
        } 
        // else if (folderId == 0 && !folderIdStr.empty()) {
        //   // 0 表示查询未分类的笔记（folder_id IS NULL）
        //   whereClause += " AND folder_id IS NULL";
        // }

        // 添加 tag_ids 过滤
        if (!tagIds.empty()) {
          // 构建标签过滤子查询
          std::string tagIdsStrForSql;
          for (size_t i = 0; i < tagIds.size(); ++i) {
            if (i > 0) tagIdsStrForSql += ",";
            tagIdsStrForSql += std::to_string(tagIds[i]);
          }

          std::string subQuery =
              " AND id IN ("
              "  SELECT note_id"
              "  FROM note_tag"
              "  WHERE tag_id IN (" +
              tagIdsStrForSql + ")"
                                "  GROUP BY note_id"
                                "  HAVING COUNT(DISTINCT tag_id) = " +
              std::to_string(tagIds.size()) +
              ")";

          whereClause += subQuery;
        }

        // 执行查询
        std::string sql = "SELECT id, title, summary, folder_id, created_at, updated_at FROM note " + whereClause;

        // 根据是否有 folderId 参数选择调用方式
        std::cout<<sql<<'\n';
        if (hasFolderParam) {
          dbClient->execSqlAsync(
              sql,
              [callback, this](const drogon::orm::Result &result)
              {
                Json::Value data(Json::arrayValue);
                for (size_t i = 0; i < result.size(); ++i) {
                  const auto &row = result[i];
                  Json::Value noteJson;
                  noteJson["id"]         = static_cast<Json::Int64>(row["id"].as<int64_t>());
                  noteJson["title"]      = row["title"].as<std::string>();
                  noteJson["summary"]    = row["summary"].isNull() ? "" : row["summary"].as<std::string>();
                  noteJson["folder_id"]  = row["folder_id"].isNull() ? 0 : static_cast<Json::Int64>(row["folder_id"].as<int64_t>());
                  noteJson["created_at"] = row["created_at"].as<std::string>();
                  noteJson["updated_at"] = row["updated_at"].as<std::string>();
                  data.append(noteJson);
                }

                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取笔记列表成功", data));
                callback(resp);
              },
              [callback, this](const drogon::orm::DrogonDbException &e)
              {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取笔记列表失败: " + std::string(e.base().what())));
                callback(resp);
              },
              userId,
              folderId);
        } else {
          dbClient->execSqlAsync(
              sql,
              [callback, this](const drogon::orm::Result &result)
              {
                Json::Value data(Json::arrayValue);
                for (size_t i = 0; i < result.size(); ++i) {
                  const auto &row = result[i];
                  Json::Value noteJson;
                  noteJson["id"]         = static_cast<Json::Int64>(row["id"].as<int64_t>());
                  noteJson["title"]      = row["title"].as<std::string>();
                  noteJson["summary"]    = row["summary"].isNull() ? "" : row["summary"].as<std::string>();
                  noteJson["folder_id"]  = row["folder_id"].isNull() ? 0 : static_cast<Json::Int64>(row["folder_id"].as<int64_t>());
                  noteJson["created_at"] = row["created_at"].as<std::string>();
                  noteJson["updated_at"] = row["updated_at"].as<std::string>();
                  data.append(noteJson);
                }

                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取笔记列表成功", data));
                callback(resp);
              },
              [callback, this](const drogon::orm::DrogonDbException &e)
              {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取笔记列表失败: " + std::string(e.base().what())));
                callback(resp);
              },
              userId);
        }
      });
}

void NoteController::getNoteDetail(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
        if (!valid) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
          callback(resp);
          return;
        }

        std::string noteIdStr = req->getParameter("note_id");
        if (noteIdStr.empty()) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记ID不能为空"));
          callback(resp);
          return;
        }

        int64_t noteId = std::stoll(noteIdStr);

        // 查询笔记详情
        drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(drogon::app().getDbClient("default"));
        noteMapper.findByPrimaryKey(
            noteId,
            [callback, this, userId](const drogon_model::calcite::Note &note)
            {
              if (note.getValueOfUserId() != userId) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问该笔记"));
                callback(resp);
                return;
              }

              Json::Value data;
              data["id"]        = static_cast<Json::Int64>(note.getValueOfId());
              data["title"]     = note.getValueOfTitle();
              data["content"]   = note.getValueOfContent();
              data["summary"]   = note.getValueOfSummary();
              data["folder_id"] = note.getFolderId() ? static_cast<Json::Int64>(note.getValueOfFolderId()) : 0;
              if (note.getCreatedAt()) {
                data["created_at"] = note.getCreatedAt()->toDbStringLocal();
              }
              if (note.getUpdatedAt()) {
                data["updated_at"] = note.getUpdatedAt()->toDbStringLocal();
              }

              auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取笔记详情成功", data));
              callback(resp);
            },
            [callback, this](const drogon::orm::DrogonDbException &e)
            {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "查询笔记失败: " + std::string(e.base().what())));
              callback(resp);
            });
      });
}

void NoteController::searchNotes(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId)
      {
        if (!valid) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
          callback(resp);
          return;
        }

        std::string keyword = req->getParameter("keyword");
        if (keyword.empty()) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "搜索关键词不能为空"));
          callback(resp);
          return;
        }

        std::string isPublicStr = req->getParameter("is_public"); // 可选参数，有就搜索公开笔记
        bool        isPublic    = false;
        if (!isPublicStr.empty()) {
          isPublic = true;
        }

        // 解析分页参数
        int from = 0;
        int size = 20;
        try {
          std::string fromStr = req->getParameter("from");
          std::string sizeStr = req->getParameter("size");
          if (!fromStr.empty()) from = std::stoi(fromStr);
          if (!sizeStr.empty()) size = std::stoi(sizeStr);
          if (size > 100) size = 100; // 限制最大返回数量
        } catch (...) {
          // 使用默认值
        }

        // 使用ES进行全文搜索，直接利用返回结果构造响应，无需二次查询数据库
        esClient_.search(userId, isPublic, keyword, 
          [this, callback](const std::vector<utils::EsSearchResult>& esResults) {
            Json::Value data(Json::arrayValue);
            for (const auto& esResult : esResults) {
              Json::Value noteJson;
              noteJson["id"] = static_cast<Json::Int64>(esResult.noteId);
              noteJson["title"] = esResult.title;
              noteJson["summary"] = esResult.summary;
              noteJson["created_at"] = esResult.createdAt;
              noteJson["updated_at"] = esResult.updatedAt;
              
              if (!esResult.highlightTitle.empty()) {
                noteJson["highlight_title"] = esResult.highlightTitle;
              }
              if (!esResult.highlightContent.empty()) {
                noteJson["highlight_content"] = esResult.highlightContent;
              }
              noteJson["score"] = esResult.score;
              
              data.append(noteJson);
            }
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "搜索成功", data));
            callback(resp);
          },
          from, size
        );
      });
}

} // namespace v1
} // namespace api
} // namespace calcite
