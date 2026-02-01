#include "NoteFolderService.h"
#include <drogon/drogon.h>
#include <algorithm>

namespace calcite {
namespace services {

NoteFolderService::NoteFolderService()
    : folderMapper_(drogon::app().getDbClient("default")) {}

void NoteFolderService::createFolder(int64_t userId,
                                      const std::string& name,
                                      int64_t parentId,
                                      std::function<void(const CreateFolderResult&)> callback) {
    if (name.empty()) {
        CreateFolderResult result;
        result.success = false;
        result.message = "文件夹名称不能为空";
        callback(result);
        return;
    }

    // 如果指定了父文件夹，验证父文件夹是否存在且属于当前用户
    if (parentId > 0) {
        folderMapper_.findBy(
            drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_id, drogon::orm::CompareOperator::EQ, parentId) &&
            drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId),
            [this, userId, name, parentId, callback](const std::vector<drogon_model::calcite::NoteFolder>& folders) {
                if (folders.empty()) {
                    CreateFolderResult result;
                    result.success = false;
                    result.message = "父文件夹不存在或无权访问";
                    callback(result);
                    return;
                }

                // 创建新文件夹
                drogon_model::calcite::NoteFolder folder;
                folder.setUserId(userId);
                folder.setName(name);
                folder.setParentId(parentId);

                folderMapper_.insert(
                    folder,
                    [callback](const drogon_model::calcite::NoteFolder& insertedFolder) {
                        CreateFolderResult result;
                        result.success = true;
                        result.message = "创建文件夹成功";
                        result.folderId = insertedFolder.getValueOfId();
                        callback(result);
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        CreateFolderResult result;
                        result.success = false;
                        result.message = "创建文件夹失败: " + std::string(e.base().what());
                        callback(result);
                    });
            },
            [callback](const drogon::orm::DrogonDbException& e) {
                CreateFolderResult result;
                result.success = false;
                result.message = "查询父文件夹失败: " + std::string(e.base().what());
                callback(result);
            });
    } else {
        // 创建根级文件夹
        drogon_model::calcite::NoteFolder folder;
        folder.setUserId(userId);
        folder.setName(name);

        folderMapper_.insert(
            folder,
            [callback](const drogon_model::calcite::NoteFolder& insertedFolder) {
                CreateFolderResult result;
                result.success = true;
                result.message = "创建文件夹成功";
                result.folderId = insertedFolder.getValueOfId();
                callback(result);
            },
            [callback](const drogon::orm::DrogonDbException& e) {
                CreateFolderResult result;
                result.success = false;
                result.message = "创建文件夹失败: " + std::string(e.base().what());
                callback(result);
            });
    }
}

void NoteFolderService::listFolders(int64_t userId,
                                    std::function<void(bool, const std::vector<FolderDetail>&, const std::string&)> callback) {
    folderMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId),
        [this, callback](const std::vector<drogon_model::calcite::NoteFolder>& folders) {
            std::vector<FolderDetail> details;
            for (const auto& folder : folders) {
                details.push_back(folderToDetail(folder));
            }
            callback(true, details, "");
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(false, {}, "获取文件夹列表失败: " + std::string(e.base().what()));
        });
}

FolderDetail NoteFolderService::folderToDetail(const drogon_model::calcite::NoteFolder& folder) {
    FolderDetail detail;
    detail.id = folder.getValueOfId();
    detail.userId = folder.getValueOfUserId();
    detail.name = folder.getValueOfName();
    detail.parentId = folder.getParentId() ? folder.getValueOfParentId() : 0;
    if (folder.getCreatedAt()) {
        detail.createdAt = folder.getCreatedAt()->toDbStringLocal();
    }
    return detail;
}

} // namespace services
} // namespace calcite
