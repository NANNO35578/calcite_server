#include "TagService.h"
#include <drogon/drogon.h>
#include <algorithm>

namespace calcite {
namespace services {

TagService::TagService()
    : tagMapper_(drogon::app().getDbClient("default")) {}

void TagService::createTag(int64_t userId,
                           const std::string& name,
                           std::function<void(const CreateTagResult&)> callback) {
    if (name.empty()) {
        CreateTagResult result;
        result.success = false;
        result.message = "标签名称不能为空";
        callback(result);
        return;
    }

    // 检查标签是否已存在
    tagMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId) &&
        drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_name, drogon::orm::CompareOperator::EQ, name),
        [this, userId, name, callback](const std::vector<drogon_model::calcite::Tag>& tags) {
            if (!tags.empty()) {
                CreateTagResult result;
                result.success = false;
                result.message = "标签已存在";
                callback(result);
                return;
            }

            // 创建新标签
            drogon_model::calcite::Tag tag;
            tag.setUserId(userId);
            tag.setName(name);

            tagMapper_.insert(
                tag,
                [callback](const drogon_model::calcite::Tag& insertedTag) {
                    CreateTagResult result;
                    result.success = true;
                    result.message = "创建标签成功";
                    result.tagId = insertedTag.getValueOfId();
                    callback(result);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    CreateTagResult result;
                    result.success = false;
                    result.message = "创建标签失败: " + std::string(e.base().what());
                    callback(result);
                });
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            CreateTagResult result;
            result.success = false;
            result.message = "查询标签失败: " + std::string(e.base().what());
            callback(result);
        });
}

void TagService::listTags(int64_t userId,
                         std::function<void(bool, const std::vector<TagDetail>&, const std::string&)> callback) {
    tagMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::Tag::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId),
        [this, callback](const std::vector<drogon_model::calcite::Tag>& tags) {
            std::vector<TagDetail> details;
            for (const auto& tag : tags) {
                details.push_back(tagToDetail(tag));
            }
            callback(true, details, "");
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(false, {}, "获取标签列表失败: " + std::string(e.base().what()));
        });
}

void TagService::bindTagsToNote(int64_t noteId,
                                int64_t userId,
                                const std::vector<int64_t>& tagIds,
                                std::function<void(const BindTagResult&)> callback) {
    auto client = drogon::app().getDbClient("default");

    // 先检查笔记是否存在且属于当前用户
    std::string checkSql = R"(
        SELECT id FROM note
        WHERE id = ? AND user_id = ? AND is_deleted = 0
    )";

    client->execSqlAsync(
        checkSql,
        [this, client, noteId, userId, tagIds, callback](const drogon::orm::Result& result) {
            if (result.size() == 0) {
                BindTagResult res;
                res.success = false;
                res.message = "笔记不存在或无权访问";
                callback(res);
                return;
            }

            // 删除该笔记的现有标签绑定
            std::string deleteSql = "DELETE FROM note_tag WHERE note_id = ?";
            client->execSqlAsync(
                deleteSql,
                [this, noteId, tagIds, callback](const drogon::orm::Result&) {
                    if (tagIds.empty()) {
                        BindTagResult res;
                        res.success = true;
                        res.message = "绑定标签成功";
                        callback(res);
                        return;
                    }

                    // 逐个插入新的标签绑定
                    auto indexPtr = std::make_shared<size_t>(0);

                    // 使用 shared_ptr 包装 std::function 实现递归调用
                    auto insertNext = std::make_shared<std::function<void()>>();
                    *insertNext = [this, noteId, tagIds, callback, indexPtr, insertNext]() {
                        size_t i = *indexPtr;
                        if (i >= tagIds.size()) {
                            // 所有标签绑定完成
                            BindTagResult res;
                            res.success = true;
                            res.message = "绑定标签成功";
                            callback(res);
                            return;
                        }

                        auto client = drogon::app().getDbClient("default");
                        client->execSqlAsync(
                            "INSERT INTO note_tag (note_id, tag_id) VALUES (?, ?)",
                            [this, indexPtr, insertNext](const drogon::orm::Result&) {
                                (*indexPtr)++;
                                (*insertNext)();
                            },
                            [callback](const drogon::orm::DrogonDbException& e) {
                                BindTagResult res;
                                res.success = false;
                                res.message = "绑定标签失败: " + std::string(e.base().what());
                                callback(res);
                            },
                            noteId, tagIds[i]);
                    };

                    (*insertNext)();
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    BindTagResult res;
                    res.success = false;
                    res.message = "删除旧标签失败: " + std::string(e.base().what());
                    callback(res);
                },
                noteId);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            BindTagResult res;
            res.success = false;
            res.message = "检查笔记失败: " + std::string(e.base().what());
            callback(res);
        },
        noteId, userId);
}

TagDetail TagService::tagToDetail(const drogon_model::calcite::Tag& tag) {
    TagDetail detail;
    detail.id = tag.getValueOfId();
    detail.userId = tag.getValueOfUserId();
    detail.name = tag.getValueOfName();
    if (tag.getCreatedAt()) {
        detail.createdAt = tag.getCreatedAt()->toDbStringLocal();
    }
    return detail;
}

} // namespace services
} // namespace calcite
