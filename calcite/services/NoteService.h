#pragma once

#include <drogon/orm/Mapper.h>
#include "../models/Note.h"
#include <string>
#include <functional>
#include <vector>

namespace calcite {
namespace services {

struct CreateNoteResult {
    bool success;
    std::string message;
    int64_t noteId;
};

struct UpdateNoteResult {
    bool success;
    std::string message;
};

struct DeleteNoteResult {
    bool success;
    std::string message;
};

struct NoteDetail {
    int64_t id;
    int64_t userId;
    std::string title;
    std::string content;
    std::string summary;
    int64_t folderId;
    std::string createdAt;
    std::string updatedAt;
};

class NoteService {
public:
    NoteService();

    // 创建笔记
    void createNote(int64_t userId,
                  const std::string& title,
                  const std::string& content,
                  const std::string& summary,
                  int64_t folderId,
                  std::function<void(const CreateNoteResult&)> callback);

    // 更新笔记
    void updateNote(int64_t noteId,
                  int64_t userId,
                  const std::string& title,
                  const std::string& content,
                  const std::string& summary,
                  int64_t folderId,
                  std::function<void(const UpdateNoteResult&)> callback);

    // 删除笔记（软删除）
    void deleteNote(int64_t noteId,
                  int64_t userId,
                  std::function<void(const DeleteNoteResult&)> callback);

    // 获取笔记列表
    void listNotes(int64_t userId,
                 int64_t folderId,
                 std::function<void(bool, const std::vector<NoteDetail>&, const std::string&)> callback);

    // 获取笔记详情
    void getNoteDetail(int64_t noteId,
                    int64_t userId,
                    std::function<void(bool, const NoteDetail&, const std::string&)> callback);

    // 搜索笔记（全文搜索）
    void searchNotes(int64_t userId,
                   const std::string& keyword,
                   std::function<void(bool, const std::vector<NoteDetail>&, const std::string&)> callback);

private:
    drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper_;
    NoteDetail noteToDetail(const drogon_model::calcite::Note& note);
};

} // namespace services
} // namespace calcite
