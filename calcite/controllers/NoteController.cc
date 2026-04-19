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

std::vector<std::string> getTopKTagInMap(const std::map<std::string, int> &tagHitCounts, size_t k) {
  std::vector<std::pair<std::string, int>> tagCountVec(tagHitCounts.begin(), tagHitCounts.end());
  std::sort(tagCountVec.begin(), tagCountVec.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
  
  std::vector<std::string> topKTags;
  for (size_t i = 0; i < std::min(k, tagCountVec.size()); ++i) {
    topKTags.push_back(tagCountVec[i].first);
  }
  return topKTags;
}

std::vector<std::string> getTop2TagInMap(const std::map<std::string, int> &tagHitCounts) {
    // 初始化：最大值、第二大值，以及对应的字符串
    int max1 = -1;
    int max2 = -1;
    std::string str1, str2;

    // 遍历 map
    for (const auto& pair : tagHitCounts) {
        int value = pair.second;

        if (value > max1) {
            // 当前值比最大值还大：原来的最大值降级为第二大
            max2 = max1;
            str2 = str1;
            max1 = value;
            str1 = pair.first;
        } else if (value > max2) {
            // 当前值比第二大值大
            max2 = value;
            str2 = pair.first;
        }
    }

    std::vector<std::string> top2Tags;
    if (!str1.empty()) {
        top2Tags.push_back(str1);
    }
    if (!str2.empty()) {
        top2Tags.push_back(str2);
    }
    return top2Tags;
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
          [this,userId, isPublic, callback](const std::vector<utils::EsSearchResult>& esResults, const std::map<std::string, int>& tagHitCounts) {

            // 查询为public才写入search_history表
            if(isPublic){
              std::vector<std::string> topTags = getTopKTagInMap(tagHitCounts,2);
              drogon::orm::Mapper<drogon_model::calcite::SearchHistory> historyMapper(drogon::app().getDbClient("default"));
              drogon_model::calcite::SearchHistory history;
              if(topTags.size() > 0){ 
                history.setUserId(userId);
                history.setQuery(topTags[0]);
                historyMapper.insert(history);
              }
              if(topTags.size() > 1){
                history.setUserId(userId);
                history.setQuery(topTags[1]);
                historyMapper.insert(history);
              }
            }


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
        if (note.getValueOfUserId() != userId && note.getValueOfIsPublic() == 0) {
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

        kimiService_.recommendTags(content, 
         [this, noteId, callback, dbClient](const calcite::services::TagRecommendationResult &result) {
        // dsService_.recommendTags(content, [this, noteId, callback, dbClient](const calcite::services::TagRecommendationResult &result) {
          if (!result.success) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "AI标签生成失败: " + result.errorMessage));
            callback(resp);
            return;
          }

          if (result.tags.empty()) {
            // 上一步日志已经记录了AI生成标签为空的情况，这里直接返回成功但空列表
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "AI标签生成成功", Json::Value(Json::arrayValue)));
            callback(resp);
            return;
          }

          drogon::orm::Mapper<drogon_model::calcite::Tag> tagMapper(dbClient);
          tagMapper.findBy(
            drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_name, drogon::orm::CompareOperator::In, result.tags),
            [this, noteId, callback, dbClient, result](const std::vector<drogon_model::calcite::Tag> &tags) {
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
                  // 删除旧关联成功后，走这里批量插入新关联
                  [this, noteId, callback, dbClient, tagIds, tagNames](size_t) {
                    // ======================
                    // 批量插入 NoteTag（原生 SQL，最稳）
                    // ======================
                    if (tagIds.empty()) {
                      esClient_.updateDocument(noteId, nullptr, nullptr, nullptr, {}, nullptr);
                      auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "AI标签生成为空", Json::arrayValue));
                      callback(resp);
                      return;
                    }

                    // 组装完整 SQL
                    std::string sql = R"(INSERT INTO note_tag (note_id, tag_id) VALUES)";
                    for(auto tid:tagIds){
                      sql+="("+std::to_string(noteId)+","+std::to_string(tid)+"),";
                    }
                    sql.pop_back(); // 去掉最后一个逗号

                    // 执行 SQL
                    dbClient->execSqlAsync(
                        sql,
                        [this, noteId, callback, tagNames](const drogon::orm::Result &res) {
                          // 全部插入成功！
                          esClient_.updateDocument(noteId, nullptr, nullptr, nullptr, tagNames, nullptr);

                          Json::Value data;
                          for (auto &name : tagNames) {
                            Json::Value obj;
                            obj["name"] = name;
                            data.append(obj);
                          }

                          auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "AI标签生成并保存成功", data));
                          callback(resp);
                        },
                        [this, callback](const drogon::orm::DrogonDbException &e)
                        {
                          LOG_ERROR << "批量插入标签失败: " << e.base().what();
                          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "保存标签失败: " + std::string(e.base().what())));
                          callback(resp);
                        }
                    );
                  },
                  [this, callback](const drogon::orm::DrogonDbException &e)
                  {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "清除旧标签关联失败: " + std::string(e.base().what())));
                    callback(resp);
                  });
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

void NoteController::getNoteTagIds(int64_t noteId, std::function<void(const std::vector<int64_t> &)> callback) {
  auto dbClient = drogon::app().getDbClient("default");
  drogon::orm::Mapper<drogon_model::calcite::NoteTag> noteTagMapper(dbClient);
  noteTagMapper.findBy(
    drogon::orm::Criteria(drogon_model::calcite::NoteTag::Cols::_note_id, noteId),
    [callback](const std::vector<drogon_model::calcite::NoteTag> &noteTags) {
      std::vector<int64_t> tagIds;
      for (const auto &nt : noteTags) {
        tagIds.push_back(nt.getValueOfTagId());
      }
      callback(tagIds);
    },
    [callback](const drogon::orm::DrogonDbException &e) {
      LOG_ERROR << "查询笔记标签失败: " << e.base().what();
      callback(std::vector<int64_t>());
    }
  );
}

void NoteController::batchUpdateTagStat(int64_t userId, const std::vector<int64_t> &tagIds, const std::string &actionType, bool isIncrement, std::function<void(bool)> callback) {
  if (tagIds.empty()) {
    callback(true);
    return;
  }

  auto dbClient = drogon::app().getDbClient("default");
  std::string sql;

  if (actionType == "view") {
    sql = "INSERT INTO user_tag_stat (user_id, tag_id, view_count, last_action_time) VALUES ";
    for (size_t i = 0; i < tagIds.size(); ++i) {
      sql += "(" + std::to_string(userId) + ", " + std::to_string(tagIds[i]) + ", 1, NOW())";
      if (i + 1 < tagIds.size()) sql += ", ";
    }
    sql += " ON DUPLICATE KEY UPDATE view_count = view_count + 1, last_action_time = NOW()";
  } else if (actionType == "like") {
    if (isIncrement) {
      sql = "INSERT INTO user_tag_stat (user_id, tag_id, like_count) VALUES ";
      for (size_t i = 0; i < tagIds.size(); ++i) {
        sql += "(" + std::to_string(userId) + ", " + std::to_string(tagIds[i]) + ", 1)";
        if (i + 1 < tagIds.size()) sql += ", ";
      }
      sql += " ON DUPLICATE KEY UPDATE like_count = like_count + 1";
    } else {
      sql = "UPDATE user_tag_stat SET like_count = GREATEST(like_count - 1, 0) WHERE user_id = " + std::to_string(userId) + " AND tag_id IN (";
      for (size_t i = 0; i < tagIds.size(); ++i) {
        sql += std::to_string(tagIds[i]);
        if (i + 1 < tagIds.size()) sql += ", ";
      }
      sql += ")";
    }
  } else if (actionType == "collect") {
    if (isIncrement) {
      sql = "INSERT INTO user_tag_stat (user_id, tag_id, collect_count) VALUES ";
      for (size_t i = 0; i < tagIds.size(); ++i) {
        sql += "(" + std::to_string(userId) + ", " + std::to_string(tagIds[i]) + ", 1)";
        if (i + 1 < tagIds.size()) sql += ", ";
      }
      sql += " ON DUPLICATE KEY UPDATE collect_count = collect_count + 1";
    } else {
      sql = "UPDATE user_tag_stat SET collect_count = GREATEST(collect_count - 1, 0) WHERE user_id = " + std::to_string(userId) + " AND tag_id IN (";
      for (size_t i = 0; i < tagIds.size(); ++i) {
        sql += std::to_string(tagIds[i]);
        if (i + 1 < tagIds.size()) sql += ", ";
      }
      sql += ")";
    }
  }

  dbClient->execSqlAsync(
    sql,
    [callback](const drogon::orm::Result &) {
      callback(true);
    },
    [callback](const drogon::orm::DrogonDbException &e) {
      LOG_ERROR << "批量更新用户标签统计失败: " << e.base().what();
      callback(false);
    }
  );
}

void NoteController::viewNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
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

    auto dbClient = drogon::app().getDbClient("default");

    drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(dbClient);
    noteMapper.findByPrimaryKey(
      noteId,
      [this, userId, noteId, callback, dbClient](const drogon_model::calcite::Note &note) {
        if (note.getValueOfUserId() != userId && note.getValueOfIsPublic() == 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问该笔记"));
          callback(resp);
          return;
        }

        drogon_model::calcite::UserAction action;
        action.setUserId(userId);
        action.setNoteId(noteId);
        action.setActionType(1);

        drogon::orm::Mapper<drogon_model::calcite::UserAction> actionMapper(dbClient);
        actionMapper.insert(
          action,
          [this, userId, noteId, callback, dbClient](const drogon_model::calcite::UserAction &) {
            dbClient->execSqlAsync(
              "UPDATE note SET view_count = view_count + 1 WHERE id = ?",
              [this, userId, noteId, callback, dbClient](const drogon::orm::Result &) {
                getNoteTagIds(noteId, [this, userId, callback](const std::vector<int64_t> &tagIds) {
                  batchUpdateTagStat(userId, tagIds, "view", true, [this, callback](bool) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "浏览笔记成功"));
                    callback(resp);
                  });
                });
              },
              [this, callback](const drogon::orm::DrogonDbException &e) {
                LOG_ERROR << "更新笔记浏览量失败: " << e.base().what();
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "浏览笔记成功"));
                callback(resp);
              },
              noteId
            );
          },
          [this, callback](const drogon::orm::DrogonDbException &e) {
            LOG_ERROR << "记录用户行为失败: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "浏览笔记成功"));
            callback(resp);
          }
        );
      },
      [this, callback](const drogon::orm::DrogonDbException &e) {
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记不存在"));
        callback(resp);
      }
    );
  });
}

void NoteController::likeNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
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

    auto dbClient = drogon::app().getDbClient("default");

    drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(dbClient);
    noteMapper.findByPrimaryKey(
      noteId,
      [this, userId, noteId, callback, dbClient](const drogon_model::calcite::Note &note) {
        if (note.getValueOfUserId() != userId && note.getValueOfIsPublic() == 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问该笔记"));
          callback(resp);
          return;
        }

        dbClient->execSqlAsync(
          "INSERT IGNORE INTO note_like (user_id, note_id) VALUES (?, ?)",
          [this, userId, noteId, callback, dbClient](const drogon::orm::Result &result) {
            if (result.affectedRows() == 0) {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "已点赞过该笔记"));
              callback(resp);
              return;
            }

            drogon_model::calcite::UserAction action;
            action.setUserId(userId);
            action.setNoteId(noteId);
            action.setActionType(2);

            drogon::orm::Mapper<drogon_model::calcite::UserAction> actionMapper(dbClient);
            actionMapper.insert(
              action,
              [this, userId, noteId, callback, dbClient](const drogon_model::calcite::UserAction &) {
                dbClient->execSqlAsync(
                  "UPDATE note SET like_count = like_count + 1 WHERE id = ?",
                  [this, userId, noteId, callback, dbClient](const drogon::orm::Result &) {
                    getNoteTagIds(noteId, [this, userId, callback](const std::vector<int64_t> &tagIds) {
                      batchUpdateTagStat(userId, tagIds, "like", true, [this, callback](bool) {
                        auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "点赞笔记成功"));
                        callback(resp);
                      });
                    });
                  },
                  [this, callback](const drogon::orm::DrogonDbException &e) {
                    LOG_ERROR << "更新笔记点赞数失败: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "点赞笔记成功"));
                    callback(resp);
                  },
                  noteId
                );
              },
              [this, callback](const drogon::orm::DrogonDbException &e) {
                LOG_ERROR << "记录用户行为失败: " << e.base().what();
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "点赞笔记成功"));
                callback(resp);
              }
            );
          },
          [this, callback](const drogon::orm::DrogonDbException &e) {
            LOG_ERROR << "插入点赞记录失败: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "点赞失败: " + std::string(e.base().what())));
            callback(resp);
          },
          userId, noteId
        );
      },
      [this, callback](const drogon::orm::DrogonDbException &e) {
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记不存在"));
        callback(resp);
      }
    );
  });
}

void NoteController::collectNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
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

    auto dbClient = drogon::app().getDbClient("default");

    drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(dbClient);
    noteMapper.findByPrimaryKey(
      noteId,
      [this, userId, noteId, callback, dbClient](const drogon_model::calcite::Note &note) {
        if (note.getValueOfUserId() != userId && note.getValueOfIsPublic() == 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问该笔记"));
          callback(resp);
          return;
        }

        dbClient->execSqlAsync(
          "INSERT IGNORE INTO note_collect (user_id, note_id) VALUES (?, ?)",
          [this, userId, noteId, callback, dbClient](const drogon::orm::Result &result) {
            if (result.affectedRows() == 0) {
              auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "已收藏过该笔记"));
              callback(resp);
              return;
            }

            drogon_model::calcite::UserAction action;
            action.setUserId(userId);
            action.setNoteId(noteId);
            action.setActionType(3);

            drogon::orm::Mapper<drogon_model::calcite::UserAction> actionMapper(dbClient);
            actionMapper.insert(
              action,
              [this, userId, noteId, callback, dbClient](const drogon_model::calcite::UserAction &) {
                dbClient->execSqlAsync(
                  "UPDATE note SET collect_count = collect_count + 1 WHERE id = ?",
                  [this, userId, noteId, callback, dbClient](const drogon::orm::Result &) {
                    getNoteTagIds(noteId, [this, userId, callback](const std::vector<int64_t> &tagIds) {
                      batchUpdateTagStat(userId, tagIds, "collect", true, [this, callback](bool) {
                        auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "收藏笔记成功"));
                        callback(resp);
                      });
                    });
                  },
                  [this, callback](const drogon::orm::DrogonDbException &e) {
                    LOG_ERROR << "更新笔记收藏数失败: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "收藏笔记成功"));
                    callback(resp);
                  },
                  noteId
                );
              },
              [this, callback](const drogon::orm::DrogonDbException &e) {
                LOG_ERROR << "记录用户行为失败: " << e.base().what();
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "收藏笔记成功"));
                callback(resp);
              }
            );
          },
          [this, callback](const drogon::orm::DrogonDbException &e) {
            LOG_ERROR << "插入收藏记录失败: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "收藏失败: " + std::string(e.base().what())));
            callback(resp);
          },
          userId, noteId
        );
      },
      [this, callback](const drogon::orm::DrogonDbException &e) {
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记不存在"));
        callback(resp);
      }
    );
  });
}

void NoteController::unlikeNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
    if (!valid) {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
      callback(resp);
      return;
    }

    int64_t noteId = 0;
    auto json = req->getJsonObject();
    if (json && json->isMember("note_id")) {
      noteId = json->get("note_id", 0).asInt64();
    } else {
      std::string noteIdStr = req->getParameter("note_id");
      if (!noteIdStr.empty()) {
        try {
          noteId = std::stoll(noteIdStr);
        } catch (...) {
          noteId = 0;
        }
      }
    }

    if (noteId <= 0) {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记ID无效"));
      callback(resp);
      return;
    }

    auto dbClient = drogon::app().getDbClient("default");

    dbClient->execSqlAsync(
      "DELETE FROM note_like WHERE user_id = ? AND note_id = ?",
      [this, userId, noteId, callback, dbClient](const drogon::orm::Result &result) {
        if (result.affectedRows() == 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "未点赞过该笔记"));
          callback(resp);
          return;
        }

        dbClient->execSqlAsync(
          "UPDATE note SET like_count = GREATEST(like_count - 1, 0) WHERE id = ?",
          [this, userId, noteId, callback, dbClient](const drogon::orm::Result &) {
            getNoteTagIds(noteId, [this, userId, callback](const std::vector<int64_t> &tagIds) {
              batchUpdateTagStat(userId, tagIds, "like", false, [this, callback](bool) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "取消点赞成功"));
                callback(resp);
              });
            });
          },
          [this, callback](const drogon::orm::DrogonDbException &e) {
            LOG_ERROR << "更新笔记点赞数失败: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "取消点赞成功"));
            callback(resp);
          },
          noteId
        );
      },
      [this, callback](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "删除点赞记录失败: " << e.base().what();
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "取消点赞失败: " + std::string(e.base().what())));
        callback(resp);
      },
      userId, noteId
    );
  });
}

void NoteController::uncollectNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
    if (!valid) {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
      callback(resp);
      return;
    }

    int64_t noteId = 0;
    auto json = req->getJsonObject();
    if (json && json->isMember("note_id")) {
      noteId = json->get("note_id", 0).asInt64();
    } else {
      std::string noteIdStr = req->getParameter("note_id");
      if (!noteIdStr.empty()) {
        try {
          noteId = std::stoll(noteIdStr);
        } catch (...) {
          noteId = 0;
        }
      }
    }

    if (noteId <= 0) {
      auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "笔记ID无效"));
      callback(resp);
      return;
    }

    auto dbClient = drogon::app().getDbClient("default");

    dbClient->execSqlAsync(
      "DELETE FROM note_collect WHERE user_id = ? AND note_id = ?",
      [this, userId, noteId, callback, dbClient](const drogon::orm::Result &result) {
        if (result.affectedRows() == 0) {
          auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "未收藏过该笔记"));
          callback(resp);
          return;
        }

        dbClient->execSqlAsync(
          "UPDATE note SET collect_count = GREATEST(collect_count - 1, 0) WHERE id = ?",
          [this, userId, noteId, callback, dbClient](const drogon::orm::Result &) {
            getNoteTagIds(noteId, [this, userId, callback](const std::vector<int64_t> &tagIds) {
              batchUpdateTagStat(userId, tagIds, "collect", false, [this, callback](bool) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "取消收藏成功"));
                callback(resp);
              });
            });
          },
          [this, callback](const drogon::orm::DrogonDbException &e) {
            LOG_ERROR << "更新笔记收藏数失败: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "取消收藏成功"));
            callback(resp);
          },
          noteId
        );
      },
      [this, callback](const drogon::orm::DrogonDbException &e) {
        LOG_ERROR << "删除收藏记录失败: " << e.base().what();
        auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "取消收藏失败: " + std::string(e.base().what())));
        callback(resp);
      },
      userId, noteId
    );
  });
}

} // namespace v1
} // namespace api
} // namespace calcite
