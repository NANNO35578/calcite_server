#pragma once

#include "../models/Note.h"
#include "../models/NoteTag.h"
#include "../models/Tag.h"
#include "../models/SearchHistory.h"
#include "../models/NoteLike.h"
#include "../models/NoteCollect.h"
#include "../models/UserAction.h"
#include "../models/UserTagStat.h"
#include "../services/AuthService.h"
#include "../services/DsService.h"
#include "../services/KimiService.h"
#include "../utils/EsClient.h"
#include <drogon/HttpController.h>

using namespace drogon;

namespace calcite {
namespace api {
namespace v1 {

class NoteController : public drogon::HttpController<NoteController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(NoteController::createNote,           "/api/note/create",   Post);
  ADD_METHOD_TO(NoteController::updateNote,           "/api/note/update",   Post);
  ADD_METHOD_TO(NoteController::deleteNote,           "/api/note/delete",   Post);
  ADD_METHOD_TO(NoteController::listNotes,            "/api/note/list",     Get);
  ADD_METHOD_TO(NoteController::getNoteDetail,        "/api/note/detail",   Get);
  ADD_METHOD_TO(NoteController::searchNotes,          "/api/note/search",   Get);
  ADD_METHOD_TO(NoteController::getNoteTagsHandler,   "/api/notes/tags",    Get);
  ADD_METHOD_TO(NoteController::generateNoteTagsByAi, "/api/notes/tags/ai", Post);
  ADD_METHOD_TO(NoteController::viewNote,             "/api/note/view",     Post);
  ADD_METHOD_TO(NoteController::likeNote,             "/api/note/like",     Post);
  ADD_METHOD_TO(NoteController::collectNote,          "/api/note/collect",  Post);
  ADD_METHOD_TO(NoteController::unlikeNote,           "/api/notes/like",    Delete);
  ADD_METHOD_TO(NoteController::uncollectNote,        "/api/notes/collect", Delete);
  METHOD_LIST_END

  void createNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void updateNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void deleteNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void listNotes(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void getNoteDetail(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void searchNotes(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void getNoteTagsHandler(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void generateNoteTagsByAi(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void viewNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void likeNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void collectNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void unlikeNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
  void uncollectNote(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

private:
  services::AuthService authService_;
  // services::DsService   dsService_;
  services::KimiService   kimiService_;
  utils::EsClient       esClient_;

  Json::Value createResponse(int code, const std::string &message, const Json::Value &data = Json::Value());
  void        verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback);
  void        getNoteTagIds(int64_t noteId, std::function<void(const std::vector<int64_t> &)> callback);
  void        batchUpdateTagStat(int64_t userId, const std::vector<int64_t> &tagIds, const std::string &actionType, bool isIncrement, std::function<void(bool)> callback);
};

} // namespace v1
} // namespace api
} // namespace calcite
