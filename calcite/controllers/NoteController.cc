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
        // std::string content  = json->get("content", "").asString();
        // std::string summary  = json->get("summary", "").asString();
        int64_t     folderId = json->get("folder_id", 0).asInt64();

        if (title.empty()) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "标题不能为空"));
          callback(resp);
          return;
        }

        // 创建笔记
        drogon_model::calcite::Note note;
        note.setUserId(userId);
        note.setTitle(title);
        note.setFolderId(folderId);
        
        drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(drogon::app().getDbClient("default"));
        noteMapper.insert(
            note,
            [this, callback, userId, title](const drogon_model::calcite::Note &insertedNote)
            {
              int64_t noteId = insertedNote.getValueOfId();
              
              // 异步索引到ES（空标签列表，因为新笔记还没有标签）默认不公开
              esClient_.indexDocument(noteId, userId, title);

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
                    
                    // TODO 标签更新后也要更新ES，目前先不处理标签更新
                    esClient_.updateDocument(noteId, pTitle, pContent, pSummary, {}, pIsPublic);
                    
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
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        // ====================== 核心：获取 folder_id ======================
        std::string folderIdStr = req->getParameter("folder_id");
        int64_t folderId = 0;
        if (!folderIdStr.empty()) {
            try {
                folderId = std::stoll(folderIdStr);
            } catch (...) {
                folderId = 0; // 非法参数默认 0
            }
        }

        // ====================== Drogon ORM 零 SQL 查询 ======================
        auto mapper = drogon::orm::Mapper<drogon_model::calcite::Note>(drogon::app().getDbClient());

        // 拼接条件：userId = ? AND isDeleted = 0 AND folderId = ?
        auto criteria = drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_user_id, userId)
                      && drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_is_deleted, 0)
                      && drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_folder_id, folderId);

        // 异步查询
        mapper.findBy(
            criteria,
            [this,callback](const std::vector<drogon_model::calcite::Note> &notes) {
                Json::Value data(Json::arrayValue);

                // 直接遍历 ORM 对象，不用处理 Result
                for (const auto &note : notes) {
                    Json::Value obj;
                    obj["id"]         = note.getValueOfId();
                    obj["title"]      = note.getValueOfTitle();
                    obj["summary"]    = note.getValueOfSummary();
                    obj["folder_id"]  = (Json::Int64)note.getValueOfFolderId();
                    obj["created_at"] = note.getValueOfCreatedAt().toDbStringLocal();
                    obj["updated_at"] = note.getValueOfUpdatedAt().toDbStringLocal();
                    data.append(obj);
                }

                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取成功", data));
                callback(resp);
            },
            [this,callback](const drogon::orm::DrogonDbException &e) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取失败：" + std::string(e.base().what())));
                callback(resp);
            }
        );
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
              if (note.getValueOfUserId() != userId && note.getIsPublic() == 0) {
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
              data["is_public"] = note.getValueOfIsPublic();

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

void NoteController::getNoteTagsHandler(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
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

    int64_t noteId;
    try {
      noteId = std::stoll(noteIdStr);
    } catch (...) {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记ID格式错误"));
      callback(resp);
      return;
    }

    auto dbClient = drogon::app().getDbClient("default");

    drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(dbClient);
    noteMapper.findByPrimaryKey(
      noteId,
      [this, noteId, userId, callback, dbClient](const drogon_model::calcite::Note &note) {
        if (note.getValueOfUserId() != userId && note.getValueOfIsPublic() == 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问该笔记"));
          callback(resp);
          return;
        }

        drogon::orm::Mapper<drogon_model::calcite::NoteTag> noteTagMapper(dbClient);
        noteTagMapper.findBy(
          drogon::orm::Criteria(drogon_model::calcite::NoteTag::Cols::_note_id, noteId),
          [this, callback, dbClient](const std::vector<drogon_model::calcite::NoteTag> &noteTags) {
            if (noteTags.empty()) {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取标签列表成功", Json::Value(Json::arrayValue)));
              callback(resp);
              return;
            }

            std::vector<int64_t> tagIds;
            for (const auto &nt : noteTags) {
              tagIds.push_back(nt.getValueOfTagId());
            }

            drogon::orm::Mapper<drogon_model::calcite::Tag> tagMapper(dbClient);
            auto criteria = drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_id, drogon::orm::CompareOperator::In, tagIds);
            tagMapper.findBy(
              criteria,
              [this, callback](const std::vector<drogon_model::calcite::Tag> &tags) {
                Json::Value data(Json::arrayValue);
                for (const auto &tag : tags) {
                  Json::Value obj;
                  obj["id"] = static_cast<Json::Int64>(tag.getValueOfId());
                  obj["name"] = tag.getValueOfName();
                  data.append(obj);
                }
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取标签列表成功", data));
                callback(resp);
              },
              [this, callback](const drogon::orm::DrogonDbException &e) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取标签详情失败: " + std::string(e.base().what())));
                callback(resp);
              });
          },
          [this, callback](const drogon::orm::DrogonDbException &e) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取标签关联失败: " + std::string(e.base().what())));
            callback(resp);
          });
      },
      [this, callback](const drogon::orm::DrogonDbException &e) {
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记不存在"));
        callback(resp);
      });
  });
}

void NoteController::generateNoteTagsByAi(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
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

    int64_t noteId;
    try {
      noteId = std::stoll(noteIdStr);
    } catch (...) {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记ID格式错误"));
      callback(resp);
      return;
    }

    auto dbClient = drogon::app().getDbClient("default");

    drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(dbClient);
    noteMapper.findByPrimaryKey(
      noteId,
      [this, noteId, userId, callback, dbClient](const drogon_model::calcite::Note &note) {
        if (note.getValueOfUserId() != userId) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问该笔记"));
          callback(resp);
          return;
        }

        std::string content = note.getValueOfContent();
        if (content.empty()) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记内容为空，无法生成标签"));
          callback(resp);
          return;
        }

        std::cout<<"crash Here?"<<'\n';
        dsService_.recommendTags(content, [this, noteId, callback, dbClient](const calcite::services::TagRecommendationResult &result) {
        std::cout<<"crash Not Here."<<'\n';
          if (!result.success) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "AI标签生成失败: " + result.errorMessage));
            callback(resp);
            return;
          }

          if (result.tags.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "AI标签生成成功", Json::Value(Json::arrayValue)));
            callback(resp);
            return;
          }

          drogon::orm::Mapper<drogon_model::calcite::Tag> tagMapper(dbClient);
          tagMapper.findBy(
            drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_name, drogon::orm::CompareOperator::In, result.tags),
            [this, noteId, callback, dbClient, result](const std::vector<drogon_model::calcite::Tag> &tags) {
              std::cout<<"crash Not Here. tags size: "<<tags.size()<<'\n';
              std::vector<int64_t> tagIds;
              std::vector<std::string> tagNames;
              for (const auto &tag : tags) {
                tagIds.push_back(tag.getValueOfId());
                tagNames.push_back(tag.getValueOfName());
              }

              if (tagIds.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "AI标签生成成功", Json::Value(Json::arrayValue)));
                callback(resp);
                return;
              }

              drogon::orm::Mapper<drogon_model::calcite::NoteTag> noteTagMapper(dbClient);
              noteTagMapper.deleteBy(
                drogon::orm::Criteria(drogon_model::calcite::NoteTag::Cols::_note_id, noteId),
                [this, noteId, callback, dbClient, tagIds, tagNames](size_t) {
                  std::function<void(size_t)> doInsert = [&](size_t index) {
                    if (index >= tagIds.size()) {
                      esClient_.updateDocument(noteId, nullptr, nullptr, nullptr, tagNames, nullptr);
                      Json::Value data(Json::arrayValue);
                      for (const auto &name : tagNames) {
                        Json::Value obj;
                        obj["name"] = name;
                        data.append(obj);
                      }
                      auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "AI标签生成并保存成功", data));
                      callback(resp);
                      return;
                    }
                    drogon_model::calcite::NoteTag nt;
                    nt.setNoteId(noteId);
                    nt.setTagId(tagIds[index]);
                    drogon::orm::Mapper<drogon_model::calcite::NoteTag> insertMapper(dbClient);
                    insertMapper.insert(
                      nt,
                      [doInsert, index](const drogon_model::calcite::NoteTag &) {
                        doInsert(index + 1);
                      },
                      [this, callback](const drogon::orm::DrogonDbException &e) {
                        LOG_ERROR << "NoteTag insert failed: " << e.base().what();
                        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "保存标签关联失败: " + std::string(e.base().what())));
                        callback(resp);
                      }
                    );
                  };
                  doInsert(0);
                },
                [this, callback](const drogon::orm::DrogonDbException &e) {
                  auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "清除旧标签关联失败: " + std::string(e.base().what())));
                  callback(resp);
                }
              );
            },
            [this, callback](const drogon::orm::DrogonDbException &e) {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "查询标签失败: " + std::string(e.base().what())));
              callback(resp);
            }
          );
        });
      },
      [this, callback](const drogon::orm::DrogonDbException &e) {
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记不存在"));
        callback(resp);
      });
  });
}

} // namespace v1
} // namespace api
} // namespace calcite
