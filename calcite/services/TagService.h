#pragma once

#include <drogon/orm/Mapper.h>
#include "../models/Tag.h"
#include <string>
#include <functional>
#include <vector>

namespace calcite {
namespace services {

struct CreateTagResult {
    bool success;
    std::string message;
    int64_t tagId;
};

struct BindTagResult {
    bool success;
    std::string message;
};

struct TagDetail {
    int64_t id;
    int64_t userId;
    std::string name;
    std::string createdAt;
};

class TagService {
public:
    TagService();

    // 创建标签
    void createTag(int64_t userId,
                   const std::string& name,
                   std::function<void(const CreateTagResult&)> callback);

    // 获取标签列表
    void listTags(int64_t userId,
                  std::function<void(bool, const std::vector<TagDetail>&, const std::string&)> callback);

    // 绑定笔记与标签
    void bindTagsToNote(int64_t noteId,
                        int64_t userId,
                        const std::vector<int64_t>& tagIds,
                        std::function<void(const BindTagResult&)> callback);

private:
    drogon::orm::Mapper<drogon_model::calcite::Tag> tagMapper_;
    TagDetail tagToDetail(const drogon_model::calcite::Tag& tag);
};

} // namespace services
} // namespace calcite
