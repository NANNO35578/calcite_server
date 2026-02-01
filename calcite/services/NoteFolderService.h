#pragma once

#include <drogon/orm/Mapper.h>
#include "../models/NoteFolder.h"
#include <string>
#include <functional>
#include <vector>

namespace calcite {
namespace services {

struct CreateFolderResult {
    bool success;
    std::string message;
    int64_t folderId;
};

struct FolderDetail {
    int64_t id;
    int64_t userId;
    std::string name;
    int64_t parentId;
    std::string createdAt;
};

class NoteFolderService {
public:
    NoteFolderService();

    // 创建文件夹
    void createFolder(int64_t userId,
                      const std::string& name,
                      int64_t parentId,
                      std::function<void(const CreateFolderResult&)> callback);

    // 获取文件夹列表
    void listFolders(int64_t userId,
                     std::function<void(bool, const std::vector<FolderDetail>&, const std::string&)> callback);

private:
    drogon::orm::Mapper<drogon_model::calcite::NoteFolder> folderMapper_;
    FolderDetail folderToDetail(const drogon_model::calcite::NoteFolder& folder);
};

} // namespace services
} // namespace calcite
