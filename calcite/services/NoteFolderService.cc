#include "NoteFolderService.h"
#include "../models/Note.h"
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
                                    int64_t parentId,
                                    std::function<void(bool, const std::vector<FolderDetail>&, const std::string&)> callback) {
    // 构建查询条件：用户ID + 父文件夹ID
    auto criteria = drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId);

    if (parentId > 0) {
        // 仅获取指定父文件夹的直接子文件夹
        criteria = criteria && drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_parent_id, drogon::orm::CompareOperator::EQ, parentId);
    } else if (parentId == 0) {
        // parentId = 0 表示获取根级文件夹（parent_id IS NULL）
        criteria = criteria && drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_parent_id, drogon::orm::CompareOperator::IsNull);
    }

    folderMapper_.findBy(
        criteria,
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

void NoteFolderService::getAllChildFolderIds(int64_t userId, int64_t folderId,
                                            std::function<void(const std::vector<int64_t>&)> callback) {
    // 查询指定文件夹下的所有子文件夹
    folderMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId) &&
            drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_parent_id, drogon::orm::CompareOperator::EQ, folderId),
        [this, userId, folderId, callback](const std::vector<drogon_model::calcite::NoteFolder>& folders) {
            std::vector<int64_t> allIds;
            allIds.push_back(folderId); // 包含当前文件夹ID

            // 递归获取所有子文件夹
            for (const auto& folder : folders) {
                int64_t childId = folder.getValueOfId();
                allIds.push_back(childId);

                // 递归查询子文件夹的子文件夹
                getAllChildFolderIds(userId, childId, [&allIds](const std::vector<int64_t>& childIds) {
                    allIds.insert(allIds.end(), childIds.begin(), childIds.end());
                });
            }

            callback(allIds);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback({}); // 返回空数组表示错误
        });
}

void NoteFolderService::updateFolder(int64_t userId,
                                     int64_t folderId,
                                     const std::string& name,
                                     int64_t parentId,
                                     std::function<void(const UpdateFolderResult&)> callback) {
    // 验证文件夹存在且属于当前用户
    folderMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_id, drogon::orm::CompareOperator::EQ, folderId) &&
            drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId),
        [this, userId, folderId, name, parentId, callback](const std::vector<drogon_model::calcite::NoteFolder>& folders) {
            if (folders.empty()) {
                UpdateFolderResult result;
                result.success = false;
                result.message = "文件夹不存在或无权访问";
                callback(result);
                return;
            }

            // 如果需要更新 parent_id，进行验证
            if (parentId >= 0) {
                if (parentId > 0) {
                    // 验证父文件夹存在且属于当前用户
                    folderMapper_.findBy(
                        drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_id, drogon::orm::CompareOperator::EQ, parentId) &&
                            drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId),
                        [this, userId, folderId, name, parentId, callback](const std::vector<drogon_model::calcite::NoteFolder>& parentFolders) {
                            if (parentFolders.empty()) {
                                UpdateFolderResult result;
                                result.success = false;
                                result.message = "父文件夹不存在或无权访问";
                                callback(result);
                                return;
                            }

                            // 检查是否将文件夹设为自己的子文件夹（防止循环引用）
                            if (parentId == folderId) {
                                UpdateFolderResult result;
                                result.success = false;
                                result.message = "不能将文件夹设置为自己的子文件夹";
                                callback(result);
                                return;
                            }

                            // 获取所有子文件夹ID，检查循环引用
                            getAllChildFolderIds(userId, folderId, [this, folderId, name, parentId, callback](const std::vector<int64_t>& childIds) {
                                // 检查 parentId 是否在子文件夹中
                                for (int64_t childId : childIds) {
                                    if (childId == parentId) {
                                        UpdateFolderResult result;
                                        result.success = false;
                                        result.message = "不能将文件夹设置为自己的子文件夹";
                                        callback(result);
                                        return;
                                    }
                                }

                                // 更新文件夹
                                drogon_model::calcite::NoteFolder folder;
                                folder.setId(folderId);
                                if (!name.empty()) {
                                    folder.setName(name);
                                }
                                if (parentId >= 0) {
                                    if (parentId == 0) {
                                        folder.setParentIdToNull();
                                    } else {
                                        folder.setParentId(parentId);
                                    }
                                }

                                folderMapper_.update(
                                    folder,
                                    [callback](const size_t count) {
                                        UpdateFolderResult result;
                                        result.success = true;
                                        result.message = "更新文件夹成功";
                                        callback(result);
                                    },
                                    [callback](const drogon::orm::DrogonDbException& e) {
                                        UpdateFolderResult result;
                                        result.success = false;
                                        result.message = "更新文件夹失败: " + std::string(e.base().what());
                                        callback(result);
                                    });
                            });
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            UpdateFolderResult result;
                            result.success = false;
                            result.message = "验证父文件夹失败: " + std::string(e.base().what());
                            callback(result);
                        });
                } else {
                    // parentId == 0，设为根文件夹
                    drogon_model::calcite::NoteFolder folder;
                    folder.setId(folderId);
                    if (!name.empty()) {
                        folder.setName(name);
                    }
                    folder.setParentIdToNull();

                    folderMapper_.update(
                        folder,
                        [callback](const size_t count) {
                            UpdateFolderResult result;
                            result.success = true;
                            result.message = "更新文件夹成功";
                            callback(result);
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            UpdateFolderResult result;
                            result.success = false;
                            result.message = "更新文件夹失败: " + std::string(e.base().what());
                            callback(result);
                        });
                }
            } else {
                // 仅更新名称
                drogon_model::calcite::NoteFolder folder;
                folder.setId(folderId);
                folder.setName(name);

                folderMapper_.update(
                    folder,
                    [callback](const size_t count) {
                        UpdateFolderResult result;
                        result.success = true;
                        result.message = "更新文件夹成功";
                        callback(result);
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        UpdateFolderResult result;
                        result.success = false;
                        result.message = "更新文件夹失败: " + std::string(e.base().what());
                        callback(result);
                    });
            }
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            UpdateFolderResult result;
            result.success = false;
            result.message = "验证文件夹失败: " + std::string(e.base().what());
            callback(result);
        });
}

void NoteFolderService::deleteFolder(int64_t userId,
                                     int64_t folderId,
                                     std::function<void(const DeleteFolderResult&)> callback) {
    // 验证文件夹存在且属于当前用户
    folderMapper_.findBy(
        drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_id, drogon::orm::CompareOperator::EQ, folderId) &&
            drogon::orm::Criteria(drogon_model::calcite::NoteFolder::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId),
        [this, userId, folderId, callback](const std::vector<drogon_model::calcite::NoteFolder>& folders) {
            if (folders.empty()) {
                DeleteFolderResult result;
                result.success = false;
                result.message = "文件夹不存在或无权访问";
                callback(result);
                return;
            }

            // 获取所有子文件夹ID
            getAllChildFolderIds(userId, folderId, [this, userId, folderId, callback](const std::vector<int64_t>& allFolderIds) {
                if (allFolderIds.empty()) {
                    DeleteFolderResult result;
                    result.success = false;
                    result.message = "获取子文件夹失败";
                    callback(result);
                    return;
                }

                // 软删除所有相关笔记
                drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(drogon::app().getDbClient("default"));

                // 构建IN条件
                auto noteCriteria = drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_user_id, drogon::orm::CompareOperator::EQ, userId);
                noteCriteria = noteCriteria && drogon::orm::Criteria(drogon_model::calcite::Note::Cols::_folder_id, drogon::orm::CompareOperator::In, allFolderIds);

                noteMapper.findBy(
                    noteCriteria,
                    [this, allFolderIds, folderId, callback](const std::vector<drogon_model::calcite::Note>& notes) {
                        // 软删除所有笔记
                        drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(drogon::app().getDbClient("default"));

                        for (const auto& note : notes) {
                            drogon_model::calcite::Note noteToDelete = note;
                            noteToDelete.setIsDeleted(1);
                            noteMapper.update(noteToDelete,
                                [](const size_t count) {},
                                [](const drogon::orm::DrogonDbException& e) {});
                        }

                        // 删除所有文件夹
                        for (int64_t fid : allFolderIds) {
                            folderMapper_.deleteByPrimaryKey(fid,
                                [](const size_t count) {},
                                [](const drogon::orm::DrogonDbException& e) {});
                        }

                        DeleteFolderResult result;
                        result.success = true;
                        result.message = "删除文件夹成功";
                        callback(result);
                    },
                    [this, allFolderIds, folderId, callback](const drogon::orm::DrogonDbException& e) {
                        DeleteFolderResult result;
                        result.success = false;
                        result.message = "查询笔记失败: " + std::string(e.base().what());
                        callback(result);
                    });
            });
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            DeleteFolderResult result;
            result.success = false;
            result.message = "验证文件夹失败: " + std::string(e.base().what());
            callback(result);
        });
}

} // namespace services
} // namespace calcite
