#include "NoteService.h"
#include <drogon/drogon.h>
#include <algorithm>

namespace calcite {
namespace services {

NoteService::NoteService()
    : noteMapper_(drogon::app().getDbClient("default")) {}

void NoteService::createNote(int64_t userId,
                          const std::string& title,
                          const std::string& content,
                          const std::string& summary,
                          int64_t folderId,
                          std::function<void(const CreateNoteResult&)> callback) {
    drogon_model::calcite::Note note;
    note.setUserId(userId);
    note.setUpdatedAt(trantor::Date::date());

    if (!title.empty()) {
        note.setTitle(title);
    }
    if (!content.empty()) {
        note.setContent(content);
    }
    if (!summary.empty()) {
        note.setSummary(summary);
    }
    if (folderId > 0) {
        note.setFolderId(folderId);
    }
    note.setIsDeleted(0);

    noteMapper_.insert(
        note,
        [callback](const drogon_model::calcite::Note& insertedNote) {
            CreateNoteResult result;
            result.success = true;
            result.message = "创建笔记成功";
            result.noteId = insertedNote.getValueOfId();
            callback(result);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            CreateNoteResult result;
            result.success = false;
            result.message = "创建笔记失败: " + std::string(e.base().what());
            callback(result);
        });
}

void NoteService::updateNote(int64_t noteId,
                          int64_t userId,
                          const std::string& title,
                          const std::string& content,
                          const std::string& summary,
                          int64_t folderId,
                          std::function<void(const UpdateNoteResult&)> callback) {
    // 先检查笔记是否存在且属于当前用户
    noteMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_id, drogon::orm::CompareOperator::EQ, noteId) &&
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId) &&
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_is_deleted, drogon::orm::CompareOperator::EQ, 0),
        [this, title, content, summary, folderId, callback](const std::vector<drogon_model::calcite::Note>& notes) {
            if (notes.empty()) {
                UpdateNoteResult result;
                result.success = false;
                result.message = "笔记不存在或无权访问";
                callback(result);
                return;
            }

            drogon_model::calcite::Note note = notes[0];
            note.setUpdatedAt(trantor::Date::date());

            if (!title.empty()) {
                note.setTitle(title);
            }
            if (!content.empty()) {
                note.setContent(content);
            }
            if (!summary.empty()) {
                note.setSummary(summary);
            }
            if (folderId >= 0) {
                if (folderId == 0) {
                    note.setFolderIdToNull();
                } else {
                    note.setFolderId(folderId);
                }
            }

            noteMapper_.update(
                note,
                [callback](const size_t count) {
                    UpdateNoteResult result;
                    result.success = true;
                    result.message = "更新笔记成功";
                    callback(result);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    UpdateNoteResult result;
                    result.success = false;
                    result.message = "更新笔记失败: " + std::string(e.base().what());
                    callback(result);
                });
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            UpdateNoteResult result;
            result.success = false;
            result.message = "查询笔记失败: " + std::string(e.base().what());
            callback(result);
        });
}

void NoteService::deleteNote(int64_t noteId,
                          int64_t userId,
                          std::function<void(const DeleteNoteResult&)> callback) {
    // 检查笔记是否存在且属于当前用户
    noteMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_id, drogon::orm::CompareOperator::EQ, noteId) &&
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId) &&
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_is_deleted, drogon::orm::CompareOperator::EQ, 0),
        [this, callback](const std::vector<drogon_model::calcite::Note>& notes) {
            if (notes.empty()) {
                DeleteNoteResult result;
                result.success = false;
                result.message = "笔记不存在或无权访问";
                callback(result);
                return;
            }

            drogon_model::calcite::Note note = notes[0];
            note.setIsDeleted(1);
            note.setUpdatedAt(trantor::Date::date());

            noteMapper_.update(
                note,
                [callback](const size_t count) {
                    DeleteNoteResult result;
                    result.success = true;
                    result.message = "删除笔记成功";
                    callback(result);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    DeleteNoteResult result;
                    result.success = false;
                    result.message = "删除笔记失败: " + std::string(e.base().what());
                    callback(result);
                });
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            DeleteNoteResult result;
            result.success = false;
            result.message = "查询笔记失败: " + std::string(e.base().what());
            callback(result);
        });
}

void NoteService::listNotes(int64_t userId,
                          int64_t folderId,
                          std::function<void(bool, const std::vector<NoteDetail>&, const std::string&)> callback) {
    // 构建查询条件
    auto criteria = drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId) &&
                 drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_is_deleted, drogon::orm::CompareOperator::EQ, 0);

    if (folderId > 0) {
        criteria = criteria && drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_folder_id, drogon::orm::CompareOperator::EQ, folderId);
    } else if (folderId == -1) {
        // -1 表示只查询未分类的笔记
        criteria = criteria && drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_folder_id, drogon::orm::CompareOperator::IsNull);
    }

    noteMapper_.findBy(
        criteria,
        [this, callback](const std::vector<drogon_model::calcite::Note>& notes) {
            std::vector<NoteDetail> details;
            for (const auto& note : notes) {
                details.push_back(noteToDetail(note));
            }
            // 按更新时间降序排列
            std::sort(details.begin(), details.end(),
                [](const NoteDetail& a, const NoteDetail& b) {
                    return a.updatedAt > b.updatedAt;
                });
            callback(true, details, "");
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(false, {}, "获取笔记列表失败: " + std::string(e.base().what()));
        });
}

void NoteService::getNoteDetail(int64_t noteId,
                             int64_t userId,
                             std::function<void(bool, const NoteDetail&, const std::string&)> callback) {
    noteMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_id, drogon::orm::CompareOperator::EQ, noteId) &&
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId) &&
        drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_is_deleted, drogon::orm::CompareOperator::EQ, 0),
        [this, callback](const std::vector<drogon_model::calcite::Note>& notes) {
            if (notes.empty()) {
                callback(false, NoteDetail{}, "笔记不存在或无权访问");
                return;
            }
            callback(true, noteToDetail(notes[0]), "");
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(false, NoteDetail{}, "获取笔记详情失败: " + std::string(e.base().what()));
        });
}

void NoteService::searchNotes(int64_t userId,
                           const std::string& keyword,
                           std::function<void(bool, const std::vector<NoteDetail>&, const std::string&)> callback) {
    // 使用 SQL 原生查询进行全文搜索
    auto client = drogon::app().getDbClient("default");
    std::string sql = R"(
        SELECT * FROM note
        WHERE user_id = ?
        AND is_deleted = 0
        AND MATCH(title, content) AGAINST(? IN BOOLEAN MODE)
        ORDER BY updated_at DESC
    )";

    client->execSqlAsync(
        sql,
        [this, callback](const drogon::orm::Result& result) {
            std::vector<NoteDetail> details;
            for (size_t i = 0; i < result.size(); ++i) {
                drogon_model::calcite::Note note(result[i], -1);
                details.push_back(noteToDetail(note));
            }
            callback(true, details, "");
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(false, {}, "搜索笔记失败: " + std::string(e.base().what()));
        },
        userId, keyword);
}

NoteDetail NoteService::noteToDetail(const drogon_model::calcite::Note& note) {
    NoteDetail detail;
    detail.id = note.getValueOfId();
    detail.userId = note.getValueOfUserId();
    detail.title = note.getValueOfTitle();
    detail.content = note.getValueOfContent();
    detail.summary = note.getValueOfSummary();
    detail.folderId = note.getFolderId() ? note.getValueOfFolderId() : 0;
    if (note.getCreatedAt()) {
        detail.createdAt = note.getCreatedAt()->toDbStringLocal();
    }
    if (note.getUpdatedAt()) {
        detail.updatedAt = note.getUpdatedAt()->toDbStringLocal();
    }
    return detail;
}

} // namespace services
} // namespace calcite
