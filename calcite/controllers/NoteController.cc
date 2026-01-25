#include "NoteController.h"
#include <drogon/drogon.h>
#include <json/json.h>

namespace calcite {
namespace api {
namespace v1 {

Json::Value NoteController::createResponse(int code, const std::string& message, const Json::Value& data) {
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

bool NoteController::verifyTokenAndGetUserId(const HttpRequestPtr& req, std::function<void(bool, int64_t)> callback) {
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

    authService_.verifyToken(token, [callback](bool valid, int64_t userId, const std::string&) {
        callback(valid, userId);
    });
    return true;
}

void NoteController::createNote(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
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

        std::string title = json->get("title", "").asString();
        std::string content = json->get("content", "").asString();
        std::string summary = json->get("summary", "").asString();
        int64_t folderId = json->get("folder_id", 0).asInt64();

        noteService_.createNote(userId, title, content, summary, folderId,
            [callback, this](const services::CreateNoteResult& result) {
                Json::Value data;
                data["note_id"] = static_cast<Json::Int64>(result.noteId);
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message, data));
                callback(resp);
            });
    });
}

void NoteController::updateNote(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
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

        if (!json->isMember("note_id")) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "缺少note_id参数"));
            callback(resp);
            return;
        }

        int64_t noteId = (*json)["note_id"].asInt64();
        std::string title = json->get("title", "").asString();
        std::string content = json->get("content", "").asString();
        std::string summary = json->get("summary", "").asString();
        int64_t folderId = json->get("folder_id", -1).asInt64();

        noteService_.updateNote(noteId, userId, title, content, summary, folderId,
            [callback, this](const services::UpdateNoteResult& result) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message));
                callback(resp);
            });
    });
}

void NoteController::deleteNote(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
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

        if (!json->isMember("note_id")) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "缺少note_id参数"));
            callback(resp);
            return;
        }

        int64_t noteId = (*json)["note_id"].asInt64();

        noteService_.deleteNote(noteId, userId,
            [callback, this](const services::DeleteNoteResult& result) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(result.success ? 0 : 1, result.message));
                callback(resp);
            });
    });
}

void NoteController::listNotes(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        std::string folderIdStr = req->getParameter("folder_id");
        int64_t folderId = folderIdStr.empty() ? 0 : std::stoll(folderIdStr);

        noteService_.listNotes(userId, folderId,
            [callback, this](bool success, const std::vector<services::NoteDetail>& notes, const std::string& message) {
                if (!success) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, message));
                    callback(resp);
                    return;
                }

                Json::Value data(Json::arrayValue);
                for (const auto& note : notes) {
                    Json::Value noteJson;
                    noteJson["id"] = static_cast<Json::Int64>(note.id);
                    noteJson["title"] = note.title;
                    noteJson["summary"] = note.summary;
                    noteJson["folder_id"] = static_cast<Json::Int64>(note.folderId);
                    noteJson["created_at"] = note.createdAt;
                    noteJson["updated_at"] = note.updatedAt;
                    data.append(noteJson);
                }

                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取笔记列表成功", data));
                callback(resp);
            });
    });
}

void NoteController::getNoteDetail(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        std::string noteIdStr = req->getParameter("note_id");
        if (noteIdStr.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "缺少note_id参数"));
            callback(resp);
            return;
        }

        int64_t noteId = std::stoll(noteIdStr);

        noteService_.getNoteDetail(noteId, userId,
            [callback, this](bool success, const services::NoteDetail& note, const std::string& message) {
                if (!success) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, message));
                    callback(resp);
                    return;
                }

                Json::Value data;
                data["id"] = static_cast<Json::Int64>(note.id);
                data["title"] = note.title;
                data["content"] = note.content;
                data["summary"] = note.summary;
                data["folder_id"] = static_cast<Json::Int64>(note.folderId);
                data["created_at"] = note.createdAt;
                data["updated_at"] = note.updatedAt;

                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取笔记详情成功", data));
                callback(resp);
            });
    });
}

void NoteController::searchNotes(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        std::string keyword = req->getParameter("keyword");
        if (keyword.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "缺少keyword参数"));
            callback(resp);
            return;
        }

        noteService_.searchNotes(userId, keyword,
            [callback, this](bool success, const std::vector<services::NoteDetail>& notes, const std::string& message) {
                if (!success) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, message));
                    callback(resp);
                    return;
                }

                Json::Value data(Json::arrayValue);
                for (const auto& note : notes) {
                    Json::Value noteJson;
                    noteJson["id"] = static_cast<Json::Int64>(note.id);
                    noteJson["title"] = note.title;
                    noteJson["summary"] = note.summary;
                    noteJson["folder_id"] = static_cast<Json::Int64>(note.folderId);
                    noteJson["created_at"] = note.createdAt;
                    noteJson["updated_at"] = note.updatedAt;
                    data.append(noteJson);
                }

                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "搜索成功", data));
                callback(resp);
            });
    });
}

} // namespace v1
} // namespace api
} // namespace calcite
