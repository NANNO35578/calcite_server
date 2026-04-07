/**
 *
 *  FileResource.h
 *  File resource model for MinIO file storage management
 *
 */

#pragma once
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/SqlBinder.h>
#include <drogon/orm/Mapper.h>
#include <drogon/orm/BaseBuilder.h>
#ifdef __cpp_impl_coroutine
#include <drogon/orm/CoroMapper.h>
#endif
#include <trantor/utils/Date.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <tuple>
#include <stdint.h>
#include <iostream>

namespace drogon
{
namespace orm
{
class DbClient;
using DbClientPtr = std::shared_ptr<DbClient>;
}
}
namespace drogon_model
{
namespace calcite
{

class FileResource
{
  public:
    struct Cols
    {
        static const std::string _id;
        static const std::string _user_id;
        static const std::string _note_id;
        static const std::string _file_name;
        static const std::string _file_path;
        static const std::string _file_type;
        static const std::string _file_size;
        static const std::string _object_key;
        static const std::string _url;
        static const std::string _status;
        static const std::string _created_at;
        static const std::string _updated_at;
    };

    static const int primaryKeyNumber;
    static const std::string tableName;
    static const bool hasPrimaryKey;
    static const std::string primaryKeyName;
    using PrimaryKeyType = int64_t;
    const PrimaryKeyType &getPrimaryKey() const;

    explicit FileResource(const drogon::orm::Row &r, const ssize_t indexOffset = 0) noexcept;
    explicit FileResource(const Json::Value &pJson) noexcept(false);
    FileResource(const Json::Value &pJson, const std::vector<std::string> &pMasqueradingVector) noexcept(false);
    FileResource() = default;

    void updateByJson(const Json::Value &pJson) noexcept(false);
    void updateByMasqueradedJson(const Json::Value &pJson,
                                 const std::vector<std::string> &pMasqueradingVector) noexcept(false);
    static bool validateJsonForCreation(const Json::Value &pJson, std::string &err);
    static bool validateMasqueradedJsonForCreation(const Json::Value &,
                                                const std::vector<std::string> &pMasqueradingVector,
                                                    std::string &err);
    static bool validateJsonForUpdate(const Json::Value &pJson, std::string &err);
    static bool validateMasqueradedJsonForUpdate(const Json::Value &,
                                          const std::vector<std::string> &pMasqueradingVector,
                                          std::string &err);
    static bool validJsonOfField(size_t index,
                          const std::string &fieldName,
                          const Json::Value &pJson,
                          std::string &err,
                          bool isForCreation);

    /**  For column id  */
    const int64_t &getValueOfId() const noexcept;
    const std::shared_ptr<int64_t> &getId() const noexcept;
    void setId(const int64_t &pId) noexcept;

    /**  For column user_id  */
    const int64_t &getValueOfUserId() const noexcept;
    const std::shared_ptr<int64_t> &getUserId() const noexcept;
    void setUserId(const int64_t &pUserId) noexcept;

    /**  For column note_id  */
    const int64_t &getValueOfNoteId() const noexcept;
    const std::shared_ptr<int64_t> &getNoteId() const noexcept;
    void setNoteId(const int64_t &pNoteId) noexcept;
    void setNoteIdToNull() noexcept;

    /**  For column file_name  */
    const std::string &getValueOfFileName() const noexcept;
    const std::shared_ptr<std::string> &getFileName() const noexcept;
    void setFileName(const std::string &pFileName) noexcept;
    void setFileName(std::string &&pFileName) noexcept;

    /**  For column file_path  */
    const std::string &getValueOfFilePath() const noexcept;
    const std::shared_ptr<std::string> &getFilePath() const noexcept;
    void setFilePath(const std::string &pFilePath) noexcept;
    void setFilePath(std::string &&pFilePath) noexcept;
    void setFilePathToNull() noexcept;

    /**  For column file_type  */
    const std::string &getValueOfFileType() const noexcept;
    const std::shared_ptr<std::string> &getFileType() const noexcept;
    void setFileType(const std::string &pFileType) noexcept;
    void setFileType(std::string &&pFileType) noexcept;
    void setFileTypeToNull() noexcept;

    /**  For column file_size  */
    const int64_t &getValueOfFileSize() const noexcept;
    const std::shared_ptr<int64_t> &getFileSize() const noexcept;
    void setFileSize(const int64_t &pFileSize) noexcept;
    void setFileSizeToNull() noexcept;

    /**  For column object_key  */
    const std::string &getValueOfObjectKey() const noexcept;
    const std::shared_ptr<std::string> &getObjectKey() const noexcept;
    void setObjectKey(const std::string &pObjectKey) noexcept;
    void setObjectKey(std::string &&pObjectKey) noexcept;

    /**  For column url  */
    const std::string &getValueOfUrl() const noexcept;
    const std::shared_ptr<std::string> &getUrl() const noexcept;
    void setUrl(const std::string &pUrl) noexcept;
    void setUrl(std::string &&pUrl) noexcept;
    void setUrlToNull() noexcept;

    /**  For column status  */
    const std::string &getValueOfStatus() const noexcept;
    const std::shared_ptr<std::string> &getStatus() const noexcept;
    void setStatus(const std::string &pStatus) noexcept;
    void setStatus(std::string &&pStatus) noexcept;

    /**  For column created_at  */
    const ::trantor::Date &getValueOfCreatedAt() const noexcept;
    const std::shared_ptr<::trantor::Date> &getCreatedAt() const noexcept;
    void setCreatedAt(const ::trantor::Date &pCreatedAt) noexcept;
    void setCreatedAtToNull() noexcept;

    /**  For column updated_at  */
    const ::trantor::Date &getValueOfUpdatedAt() const noexcept;
    const std::shared_ptr<::trantor::Date> &getUpdatedAt() const noexcept;
    void setUpdatedAt(const ::trantor::Date &pUpdatedAt) noexcept;
    void setUpdatedAtToNull() noexcept;

    static size_t getColumnNumber() noexcept { return 12; }
    static const std::string &getColumnName(size_t index) noexcept(false);

    Json::Value toJson() const;
    std::string toString() const;
    Json::Value toMasqueradedJson(const std::vector<std::string> &pMasqueradingVector) const;

  private:
    friend drogon::orm::Mapper<FileResource>;
    friend drogon::orm::BaseBuilder<FileResource, true, true>;
    friend drogon::orm::BaseBuilder<FileResource, true, false>;
    friend drogon::orm::BaseBuilder<FileResource, false, true>;
    friend drogon::orm::BaseBuilder<FileResource, false, false>;
#ifdef __cpp_impl_coroutine
    friend drogon::orm::CoroMapper<FileResource>;
#endif
    static const std::vector<std::string> &insertColumns() noexcept;
    void outputArgs(drogon::orm::internal::SqlBinder &binder) const;
    const std::vector<std::string> updateColumns() const;
    void updateArgs(drogon::orm::internal::SqlBinder &binder) const;
    void updateId(const uint64_t id);

    std::shared_ptr<int64_t> id_;
    std::shared_ptr<int64_t> userId_;
    std::shared_ptr<int64_t> noteId_;
    std::shared_ptr<std::string> fileName_;
    std::shared_ptr<std::string> filePath_;
    std::shared_ptr<std::string> fileType_;
    std::shared_ptr<int64_t> fileSize_;
    std::shared_ptr<std::string> objectKey_;
    std::shared_ptr<std::string> url_;
    std::shared_ptr<std::string> status_;
    std::shared_ptr<::trantor::Date> createdAt_;
    std::shared_ptr<::trantor::Date> updatedAt_;

    struct MetaData
    {
        const std::string colName_;
        const std::string colType_;
        const std::string colDatabaseType_;
        const ssize_t colLength_;
        const bool isAutoVal_;
        const bool isPrimaryKey_;
        const bool notNull_;
    };
    static const std::vector<MetaData> metaData_;
    bool dirtyFlag_[12] = { false };

  public:
    static const std::string &sqlForFindingByPrimaryKey()
    {
        static const std::string sql = "select * from " + tableName + " where id = ?";
        return sql;
    }

    static const std::string &sqlForDeletingByPrimaryKey()
    {
        static const std::string sql = "delete from " + tableName + " where id = ?";
        return sql;
    }

    std::string sqlForInserting(bool &needSelection) const
    {
        std::string sql = "insert into " + tableName + " (";
        size_t parametersCount = 0;
        needSelection = false;
        sql += "id,";
        ++parametersCount;
        if (dirtyFlag_[1])
        {
            sql += "user_id,";
            ++parametersCount;
        }
        if (dirtyFlag_[2])
        {
            sql += "note_id,";
            ++parametersCount;
        }
        if (dirtyFlag_[3])
        {
            sql += "file_name,";
            ++parametersCount;
        }
        if (dirtyFlag_[4])
        {
            sql += "file_path,";
            ++parametersCount;
        }
        if (dirtyFlag_[5])
        {
            sql += "file_type,";
            ++parametersCount;
        }
        if (dirtyFlag_[6])
        {
            sql += "file_size,";
            ++parametersCount;
        }
        if (dirtyFlag_[7])
        {
            sql += "object_key,";
            ++parametersCount;
        }
        if (dirtyFlag_[8])
        {
            sql += "url,";
            ++parametersCount;
        }
        sql += "status,";
        ++parametersCount;
        if (!dirtyFlag_[10])
        {
            needSelection = true;
        }
        if (dirtyFlag_[11])
        {
            sql += "updated_at,";
            ++parametersCount;
        }
        sql += "created_at,";
        ++parametersCount;
        if (!dirtyFlag_[11])
        {
            needSelection = true;
        }
        needSelection = true;
        if (parametersCount > 0)
        {
            sql[sql.length() - 1] = ')';
            sql += " values (";
        }
        else
            sql += ") values (";

        sql += "default,";
        if (dirtyFlag_[1])
        {
            sql.append("?,");
        }
        if (dirtyFlag_[2])
        {
            sql.append("?,");
        }
        if (dirtyFlag_[3])
        {
            sql.append("?,");
        }
        if (dirtyFlag_[4])
        {
            sql.append("?,");
        }
        if (dirtyFlag_[5])
        {
            sql.append("?,");
        }
        if (dirtyFlag_[6])
        {
            sql.append("?,");
        }
        if (dirtyFlag_[7])
        {
            sql.append("?,");
        }
        if (dirtyFlag_[8])
        {
            sql.append("?,");
        }
        if (dirtyFlag_[9])
        {
            sql.append("?,");
        }
        else
        {
            sql += "default,";
        }
        if (dirtyFlag_[10])
        {
            sql.append("?,");
        }
        else
        {
            sql += "default,";
        }
        if (dirtyFlag_[11])
        {
            sql.append("?,");
        }
        else
        {
            sql += "default,";
        }
        if (parametersCount > 0)
        {
            sql.resize(sql.length() - 1);
        }
        sql.append(1, ')');
        LOG_TRACE << sql;
        return sql;
    }
};

} // namespace calcite
} // namespace drogon_model
