/**
 *
 *  FileResource.cc
 *  File resource model implementation
 *
 */

#include "FileResource.h"
#include <drogon/utils/Utilities.h>
#include <string>

using namespace drogon;
using namespace drogon::orm;
using namespace drogon_model::calcite;

const std::string FileResource::Cols::_id = "id";
const std::string FileResource::Cols::_user_id = "user_id";
const std::string FileResource::Cols::_note_id = "note_id";
const std::string FileResource::Cols::_file_name = "file_name";
const std::string FileResource::Cols::_file_path = "file_path";
const std::string FileResource::Cols::_file_type = "file_type";
const std::string FileResource::Cols::_file_size = "file_size";
const std::string FileResource::Cols::_object_key = "object_key";
const std::string FileResource::Cols::_url = "url";
const std::string FileResource::Cols::_status = "status";
const std::string FileResource::Cols::_created_at = "created_at";
const std::string FileResource::Cols::_updated_at = "updated_at";

const std::string FileResource::primaryKeyName = "id";
const bool FileResource::hasPrimaryKey = true;
const std::string FileResource::tableName = "file_resource";

const std::vector<typename FileResource::MetaData> FileResource::metaData_ = {
    {"id", "int64_t", "bigint(20)", 8, 1, 1, 1},
    {"user_id", "int64_t", "bigint(20)", 8, 0, 0, 1},
    {"note_id", "int64_t", "bigint(20)", 8, 0, 0, 0},
    {"file_name", "std::string", "varchar(255)", 255, 0, 0, 1},
    {"file_path", "std::string", "varchar(512)", 512, 0, 0, 0},
    {"file_type", "std::string", "varchar(50)", 50, 0, 0, 0},
    {"file_size", "int64_t", "bigint(20)", 8, 0, 0, 0},
    {"object_key", "std::string", "varchar(255)", 255, 0, 0, 1},
    {"url", "std::string", "varchar(512)", 512, 0, 0, 0},
    {"status", "std::string", "enum('processing','done','failed')", 0, 0, 0, 0},
    {"created_at", "::trantor::Date", "datetime", 0, 0, 0, 0},
    {"updated_at", "::trantor::Date", "datetime", 0, 0, 0, 0}
};

const std::string &FileResource::getColumnName(size_t index) noexcept(false)
{
    assert(index < metaData_.size());
    return metaData_[index].colName_;
}

FileResource::FileResource(const Row &r, const ssize_t indexOffset) noexcept
{
    if (indexOffset < 0)
    {
        if (!r["id"].isNull())
        {
            id_ = std::make_shared<int64_t>(r["id"].as<int64_t>());
        }
        if (!r["user_id"].isNull())
        {
            userId_ = std::make_shared<int64_t>(r["user_id"].as<int64_t>());
        }
        if (!r["note_id"].isNull())
        {
            noteId_ = std::make_shared<int64_t>(r["note_id"].as<int64_t>());
        }
        if (!r["file_name"].isNull())
        {
            fileName_ = std::make_shared<std::string>(r["file_name"].as<std::string>());
        }
        if (!r["file_path"].isNull())
        {
            filePath_ = std::make_shared<std::string>(r["file_path"].as<std::string>());
        }
        if (!r["file_type"].isNull())
        {
            fileType_ = std::make_shared<std::string>(r["file_type"].as<std::string>());
        }
        if (!r["file_size"].isNull())
        {
            fileSize_ = std::make_shared<int64_t>(r["file_size"].as<int64_t>());
        }
        if (!r["object_key"].isNull())
        {
            objectKey_ = std::make_shared<std::string>(r["object_key"].as<std::string>());
        }
        if (!r["url"].isNull())
        {
            url_ = std::make_shared<std::string>(r["url"].as<std::string>());
        }
        if (!r["status"].isNull())
        {
            status_ = std::make_shared<std::string>(r["status"].as<std::string>());
        }
        if (!r["created_at"].isNull())
        {
            auto timeStr = r["created_at"].as<std::string>();
            struct tm stm;
            memset(&stm, 0, sizeof(stm));
            auto p = strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &stm);
            time_t t = mktime(&stm);
            size_t decimalNum = 0;
            if (p)
            {
                if (*p == '.')
                {
                    std::string decimals(p + 1, &timeStr[timeStr.length()]);
                    while (decimals.length() < 6)
                    {
                        decimals += "0";
                    }
                    decimalNum = (size_t)atol(decimals.c_str());
                }
                createdAt_ = std::make_shared<::trantor::Date>(t * 1000000 + decimalNum);
            }
        }
        if (!r["updated_at"].isNull())
        {
            auto timeStr = r["updated_at"].as<std::string>();
            struct tm stm;
            memset(&stm, 0, sizeof(stm));
            auto p = strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &stm);
            time_t t = mktime(&stm);
            size_t decimalNum = 0;
            if (p)
            {
                if (*p == '.')
                {
                    std::string decimals(p + 1, &timeStr[timeStr.length()]);
                    while (decimals.length() < 6)
                    {
                        decimals += "0";
                    }
                    decimalNum = (size_t)atol(decimals.c_str());
                }
                updatedAt_ = std::make_shared<::trantor::Date>(t * 1000000 + decimalNum);
            }
        }
    }
    else
    {
        size_t offset = (size_t)indexOffset;
        if (offset + 12 > r.size())
        {
            LOG_FATAL << "Invalid SQL result for this model";
            return;
        }
        size_t index;
        index = offset + 0;
        if (!r[index].isNull())
        {
            id_ = std::make_shared<int64_t>(r[index].as<int64_t>());
        }
        index = offset + 1;
        if (!r[index].isNull())
        {
            userId_ = std::make_shared<int64_t>(r[index].as<int64_t>());
        }
        index = offset + 2;
        if (!r[index].isNull())
        {
            noteId_ = std::make_shared<int64_t>(r[index].as<int64_t>());
        }
        index = offset + 3;
        if (!r[index].isNull())
        {
            fileName_ = std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 4;
        if (!r[index].isNull())
        {
            filePath_ = std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 5;
        if (!r[index].isNull())
        {
            fileType_ = std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 6;
        if (!r[index].isNull())
        {
            fileSize_ = std::make_shared<int64_t>(r[index].as<int64_t>());
        }
        index = offset + 7;
        if (!r[index].isNull())
        {
            objectKey_ = std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 8;
        if (!r[index].isNull())
        {
            url_ = std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 9;
        if (!r[index].isNull())
        {
            status_ = std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 10;
        if (!r[index].isNull())
        {
            auto timeStr = r[index].as<std::string>();
            struct tm stm;
            memset(&stm, 0, sizeof(stm));
            auto p = strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &stm);
            time_t t = mktime(&stm);
            size_t decimalNum = 0;
            if (p)
            {
                if (*p == '.')
                {
                    std::string decimals(p + 1, &timeStr[timeStr.length()]);
                    while (decimals.length() < 6)
                    {
                        decimals += "0";
                    }
                    decimalNum = (size_t)atol(decimals.c_str());
                }
                createdAt_ = std::make_shared<::trantor::Date>(t * 1000000 + decimalNum);
            }
        }
        index = offset + 11;
        if (!r[index].isNull())
        {
            auto timeStr = r[index].as<std::string>();
            struct tm stm;
            memset(&stm, 0, sizeof(stm));
            auto p = strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &stm);
            time_t t = mktime(&stm);
            size_t decimalNum = 0;
            if (p)
            {
                if (*p == '.')
                {
                    std::string decimals(p + 1, &timeStr[timeStr.length()]);
                    while (decimals.length() < 6)
                    {
                        decimals += "0";
                    }
                    decimalNum = (size_t)atol(decimals.c_str());
                }
                updatedAt_ = std::make_shared<::trantor::Date>(t * 1000000 + decimalNum);
            }
        }
    }
}

FileResource::FileResource(const Json::Value &pJson) noexcept(false)
{
    if (pJson.isMember("id"))
    {
        dirtyFlag_[0] = true;
        if (!pJson["id"].isNull())
        {
            id_ = std::make_shared<int64_t>((int64_t)pJson["id"].asInt64());
        }
    }
    if (pJson.isMember("user_id"))
    {
        dirtyFlag_[1] = true;
        if (!pJson["user_id"].isNull())
        {
            userId_ = std::make_shared<int64_t>((int64_t)pJson["user_id"].asInt64());
        }
    }
    if (pJson.isMember("note_id"))
    {
        dirtyFlag_[2] = true;
        if (!pJson["note_id"].isNull())
        {
            noteId_ = std::make_shared<int64_t>((int64_t)pJson["note_id"].asInt64());
        }
    }
    if (pJson.isMember("file_name"))
    {
        dirtyFlag_[3] = true;
        if (!pJson["file_name"].isNull())
        {
            fileName_ = std::make_shared<std::string>(pJson["file_name"].asString());
        }
    }
    if (pJson.isMember("file_path"))
    {
        dirtyFlag_[4] = true;
        if (!pJson["file_path"].isNull())
        {
            filePath_ = std::make_shared<std::string>(pJson["file_path"].asString());
        }
    }
    if (pJson.isMember("file_type"))
    {
        dirtyFlag_[5] = true;
        if (!pJson["file_type"].isNull())
        {
            fileType_ = std::make_shared<std::string>(pJson["file_type"].asString());
        }
    }
    if (pJson.isMember("file_size"))
    {
        dirtyFlag_[6] = true;
        if (!pJson["file_size"].isNull())
        {
            fileSize_ = std::make_shared<int64_t>((int64_t)pJson["file_size"].asInt64());
        }
    }
    if (pJson.isMember("object_key"))
    {
        dirtyFlag_[7] = true;
        if (!pJson["object_key"].isNull())
        {
            objectKey_ = std::make_shared<std::string>(pJson["object_key"].asString());
        }
    }
    if (pJson.isMember("url"))
    {
        dirtyFlag_[8] = true;
        if (!pJson["url"].isNull())
        {
            url_ = std::make_shared<std::string>(pJson["url"].asString());
        }
    }
    if (pJson.isMember("status"))
    {
        dirtyFlag_[9] = true;
        if (!pJson["status"].isNull())
        {
            status_ = std::make_shared<std::string>(pJson["status"].asString());
        }
    }
    if (pJson.isMember("created_at"))
    {
        dirtyFlag_[10] = true;
        if (!pJson["created_at"].isNull())
        {
            auto timeStr = pJson["created_at"].asString();
            struct tm stm;
            memset(&stm, 0, sizeof(stm));
            auto p = strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &stm);
            time_t t = mktime(&stm);
            size_t decimalNum = 0;
            if (p)
            {
                if (*p == '.')
                {
                    std::string decimals(p + 1, &timeStr[timeStr.length()]);
                    while (decimals.length() < 6)
                    {
                        decimals += "0";
                    }
                    decimalNum = (size_t)atol(decimals.c_str());
                }
                createdAt_ = std::make_shared<::trantor::Date>(t * 1000000 + decimalNum);
            }
        }
    }
    if (pJson.isMember("updated_at"))
    {
        dirtyFlag_[11] = true;
        if (!pJson["updated_at"].isNull())
        {
            auto timeStr = pJson["updated_at"].asString();
            struct tm stm;
            memset(&stm, 0, sizeof(stm));
            auto p = strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &stm);
            time_t t = mktime(&stm);
            size_t decimalNum = 0;
            if (p)
            {
                if (*p == '.')
                {
                    std::string decimals(p + 1, &timeStr[timeStr.length()]);
                    while (decimals.length() < 6)
                    {
                        decimals += "0";
                    }
                    decimalNum = (size_t)atol(decimals.c_str());
                }
                updatedAt_ = std::make_shared<::trantor::Date>(t * 1000000 + decimalNum);
            }
        }
    }
}

void FileResource::updateByJson(const Json::Value &pJson) noexcept(false)
{
    if (pJson.isMember("id"))
    {
        if (!pJson["id"].isNull())
        {
            id_ = std::make_shared<int64_t>((int64_t)pJson["id"].asInt64());
        }
    }
    if (pJson.isMember("user_id"))
    {
        dirtyFlag_[1] = true;
        if (!pJson["user_id"].isNull())
        {
            userId_ = std::make_shared<int64_t>((int64_t)pJson["user_id"].asInt64());
        }
    }
    if (pJson.isMember("note_id"))
    {
        dirtyFlag_[2] = true;
        if (!pJson["note_id"].isNull())
        {
            noteId_ = std::make_shared<int64_t>((int64_t)pJson["note_id"].asInt64());
        }
    }
    if (pJson.isMember("file_name"))
    {
        dirtyFlag_[3] = true;
        if (!pJson["file_name"].isNull())
        {
            fileName_ = std::make_shared<std::string>(pJson["file_name"].asString());
        }
    }
    if (pJson.isMember("file_path"))
    {
        dirtyFlag_[4] = true;
        if (!pJson["file_path"].isNull())
        {
            filePath_ = std::make_shared<std::string>(pJson["file_path"].asString());
        }
    }
    if (pJson.isMember("file_type"))
    {
        dirtyFlag_[5] = true;
        if (!pJson["file_type"].isNull())
        {
            fileType_ = std::make_shared<std::string>(pJson["file_type"].asString());
        }
    }
    if (pJson.isMember("file_size"))
    {
        dirtyFlag_[6] = true;
        if (!pJson["file_size"].isNull())
        {
            fileSize_ = std::make_shared<int64_t>((int64_t)pJson["file_size"].asInt64());
        }
    }
    if (pJson.isMember("object_key"))
    {
        dirtyFlag_[7] = true;
        if (!pJson["object_key"].isNull())
        {
            objectKey_ = std::make_shared<std::string>(pJson["object_key"].asString());
        }
    }
    if (pJson.isMember("url"))
    {
        dirtyFlag_[8] = true;
        if (!pJson["url"].isNull())
        {
            url_ = std::make_shared<std::string>(pJson["url"].asString());
        }
    }
    if (pJson.isMember("status"))
    {
        dirtyFlag_[9] = true;
        if (!pJson["status"].isNull())
        {
            status_ = std::make_shared<std::string>(pJson["status"].asString());
        }
    }
    if (pJson.isMember("created_at"))
    {
        dirtyFlag_[10] = true;
        if (!pJson["created_at"].isNull())
        {
            auto timeStr = pJson["created_at"].asString();
            struct tm stm;
            memset(&stm, 0, sizeof(stm));
            auto p = strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &stm);
            time_t t = mktime(&stm);
            size_t decimalNum = 0;
            if (p)
            {
                if (*p == '.')
                {
                    std::string decimals(p + 1, &timeStr[timeStr.length()]);
                    while (decimals.length() < 6)
                    {
                        decimals += "0";
                    }
                    decimalNum = (size_t)atol(decimals.c_str());
                }
                createdAt_ = std::make_shared<::trantor::Date>(t * 1000000 + decimalNum);
            }
        }
    }
    if (pJson.isMember("updated_at"))
    {
        dirtyFlag_[11] = true;
        if (!pJson["updated_at"].isNull())
        {
            auto timeStr = pJson["updated_at"].asString();
            struct tm stm;
            memset(&stm, 0, sizeof(stm));
            auto p = strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &stm);
            time_t t = mktime(&stm);
            size_t decimalNum = 0;
            if (p)
            {
                if (*p == '.')
                {
                    std::string decimals(p + 1, &timeStr[timeStr.length()]);
                    while (decimals.length() < 6)
                    {
                        decimals += "0";
                    }
                    decimalNum = (size_t)atol(decimals.c_str());
                }
                updatedAt_ = std::make_shared<::trantor::Date>(t * 1000000 + decimalNum);
            }
        }
    }
}

const int64_t &FileResource::getValueOfId() const noexcept
{
    static const int64_t defaultValue = int64_t();
    if (id_)
        return *id_;
    return defaultValue;
}

const std::shared_ptr<int64_t> &FileResource::getId() const noexcept
{
    return id_;
}

void FileResource::setId(const int64_t &pId) noexcept
{
    id_ = std::make_shared<int64_t>(pId);
    dirtyFlag_[0] = true;
}

const typename FileResource::PrimaryKeyType & FileResource::getPrimaryKey() const
{
    assert(id_);
    return *id_;
}

const int64_t &FileResource::getValueOfUserId() const noexcept
{
    static const int64_t defaultValue = int64_t();
    if (userId_)
        return *userId_;
    return defaultValue;
}

const std::shared_ptr<int64_t> &FileResource::getUserId() const noexcept
{
    return userId_;
}

void FileResource::setUserId(const int64_t &pUserId) noexcept
{
    userId_ = std::make_shared<int64_t>(pUserId);
    dirtyFlag_[1] = true;
}

const int64_t &FileResource::getValueOfNoteId() const noexcept
{
    static const int64_t defaultValue = int64_t();
    if (noteId_)
        return *noteId_;
    return defaultValue;
}

const std::shared_ptr<int64_t> &FileResource::getNoteId() const noexcept
{
    return noteId_;
}

void FileResource::setNoteId(const int64_t &pNoteId) noexcept
{
    noteId_ = std::make_shared<int64_t>(pNoteId);
    dirtyFlag_[2] = true;
}

void FileResource::setNoteIdToNull() noexcept
{
    noteId_.reset();
    dirtyFlag_[2] = true;
}

const std::string &FileResource::getValueOfFileName() const noexcept
{
    static const std::string defaultValue = std::string();
    if (fileName_)
        return *fileName_;
    return defaultValue;
}

const std::shared_ptr<std::string> &FileResource::getFileName() const noexcept
{
    return fileName_;
}

void FileResource::setFileName(const std::string &pFileName) noexcept
{
    fileName_ = std::make_shared<std::string>(pFileName);
    dirtyFlag_[3] = true;
}

void FileResource::setFileName(std::string &&pFileName) noexcept
{
    fileName_ = std::make_shared<std::string>(std::move(pFileName));
    dirtyFlag_[3] = true;
}

const std::string &FileResource::getValueOfFilePath() const noexcept
{
    static const std::string defaultValue = std::string();
    if (filePath_)
        return *filePath_;
    return defaultValue;
}

const std::shared_ptr<std::string> &FileResource::getFilePath() const noexcept
{
    return filePath_;
}

void FileResource::setFilePath(const std::string &pFilePath) noexcept
{
    filePath_ = std::make_shared<std::string>(pFilePath);
    dirtyFlag_[4] = true;
}

void FileResource::setFilePath(std::string &&pFilePath) noexcept
{
    filePath_ = std::make_shared<std::string>(std::move(pFilePath));
    dirtyFlag_[4] = true;
}

void FileResource::setFilePathToNull() noexcept
{
    filePath_.reset();
    dirtyFlag_[4] = true;
}

const std::string &FileResource::getValueOfFileType() const noexcept
{
    static const std::string defaultValue = std::string();
    if (fileType_)
        return *fileType_;
    return defaultValue;
}

const std::shared_ptr<std::string> &FileResource::getFileType() const noexcept
{
    return fileType_;
}

void FileResource::setFileType(const std::string &pFileType) noexcept
{
    fileType_ = std::make_shared<std::string>(pFileType);
    dirtyFlag_[5] = true;
}

void FileResource::setFileType(std::string &&pFileType) noexcept
{
    fileType_ = std::make_shared<std::string>(std::move(pFileType));
    dirtyFlag_[5] = true;
}

void FileResource::setFileTypeToNull() noexcept
{
    fileType_.reset();
    dirtyFlag_[5] = true;
}

const int64_t &FileResource::getValueOfFileSize() const noexcept
{
    static const int64_t defaultValue = int64_t();
    if (fileSize_)
        return *fileSize_;
    return defaultValue;
}

const std::shared_ptr<int64_t> &FileResource::getFileSize() const noexcept
{
    return fileSize_;
}

void FileResource::setFileSize(const int64_t &pFileSize) noexcept
{
    fileSize_ = std::make_shared<int64_t>(pFileSize);
    dirtyFlag_[6] = true;
}

void FileResource::setFileSizeToNull() noexcept
{
    fileSize_.reset();
    dirtyFlag_[6] = true;
}

const std::string &FileResource::getValueOfObjectKey() const noexcept
{
    static const std::string defaultValue = std::string();
    if (objectKey_)
        return *objectKey_;
    return defaultValue;
}

const std::shared_ptr<std::string> &FileResource::getObjectKey() const noexcept
{
    return objectKey_;
}

void FileResource::setObjectKey(const std::string &pObjectKey) noexcept
{
    objectKey_ = std::make_shared<std::string>(pObjectKey);
    dirtyFlag_[7] = true;
}

void FileResource::setObjectKey(std::string &&pObjectKey) noexcept
{
    objectKey_ = std::make_shared<std::string>(std::move(pObjectKey));
    dirtyFlag_[7] = true;
}

const std::string &FileResource::getValueOfUrl() const noexcept
{
    static const std::string defaultValue = std::string();
    if (url_)
        return *url_;
    return defaultValue;
}

const std::shared_ptr<std::string> &FileResource::getUrl() const noexcept
{
    return url_;
}

void FileResource::setUrl(const std::string &pUrl) noexcept
{
    url_ = std::make_shared<std::string>(pUrl);
    dirtyFlag_[8] = true;
}

void FileResource::setUrl(std::string &&pUrl) noexcept
{
    url_ = std::make_shared<std::string>(std::move(pUrl));
    dirtyFlag_[8] = true;
}

void FileResource::setUrlToNull() noexcept
{
    url_.reset();
    dirtyFlag_[8] = true;
}

const std::string &FileResource::getValueOfStatus() const noexcept
{
    static const std::string defaultValue = std::string();
    if (status_)
        return *status_;
    return defaultValue;
}

const std::shared_ptr<std::string> &FileResource::getStatus() const noexcept
{
    return status_;
}

void FileResource::setStatus(const std::string &pStatus) noexcept
{
    status_ = std::make_shared<std::string>(pStatus);
    dirtyFlag_[9] = true;
}

void FileResource::setStatus(std::string &&pStatus) noexcept
{
    status_ = std::make_shared<std::string>(std::move(pStatus));
    dirtyFlag_[9] = true;
}

const ::trantor::Date &FileResource::getValueOfCreatedAt() const noexcept
{
    static const ::trantor::Date defaultValue = ::trantor::Date();
    if (createdAt_)
        return *createdAt_;
    return defaultValue;
}

const std::shared_ptr<::trantor::Date> &FileResource::getCreatedAt() const noexcept
{
    return createdAt_;
}

void FileResource::setCreatedAt(const ::trantor::Date &pCreatedAt) noexcept
{
    createdAt_ = std::make_shared<::trantor::Date>(pCreatedAt);
    dirtyFlag_[10] = true;
}

void FileResource::setCreatedAtToNull() noexcept
{
    createdAt_.reset();
    dirtyFlag_[10] = true;
}

const ::trantor::Date &FileResource::getValueOfUpdatedAt() const noexcept
{
    static const ::trantor::Date defaultValue = ::trantor::Date();
    if (updatedAt_)
        return *updatedAt_;
    return defaultValue;
}

const std::shared_ptr<::trantor::Date> &FileResource::getUpdatedAt() const noexcept
{
    return updatedAt_;
}

void FileResource::setUpdatedAt(const ::trantor::Date &pUpdatedAt) noexcept
{
    updatedAt_ = std::make_shared<::trantor::Date>(pUpdatedAt);
    dirtyFlag_[11] = true;
}

void FileResource::setUpdatedAtToNull() noexcept
{
    updatedAt_.reset();
    dirtyFlag_[11] = true;
}

void FileResource::updateId(const uint64_t id)
{
    id_ = std::make_shared<int64_t>(static_cast<int64_t>(id));
}

const std::vector<std::string> &FileResource::insertColumns() noexcept
{
    static const std::vector<std::string> inCols = {
        "user_id",
        "note_id",
        "file_name",
        "file_path",
        "file_type",
        "file_size",
        "object_key",
        "url",
        "status",
        "created_at",
        "updated_at"
    };
    return inCols;
}

void FileResource::outputArgs(drogon::orm::internal::SqlBinder &binder) const
{
    if (dirtyFlag_[1])
    {
        if (getUserId())
        {
            binder << getValueOfUserId();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[2])
    {
        if (getNoteId())
        {
            binder << getValueOfNoteId();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[3])
    {
        if (getFileName())
        {
            binder << getValueOfFileName();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[4])
    {
        if (getFilePath())
        {
            binder << getValueOfFilePath();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[5])
    {
        if (getFileType())
        {
            binder << getValueOfFileType();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[6])
    {
        if (getFileSize())
        {
            binder << getValueOfFileSize();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[7])
    {
        if (getObjectKey())
        {
            binder << getValueOfObjectKey();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[8])
    {
        if (getUrl())
        {
            binder << getValueOfUrl();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[9])
    {
        if (getStatus())
        {
            binder << getValueOfStatus();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[10])
    {
        if (getCreatedAt())
        {
            binder << getValueOfCreatedAt();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[11])
    {
        if (getUpdatedAt())
        {
            binder << getValueOfUpdatedAt();
        }
        else
        {
            binder << nullptr;
        }
    }
}

const std::vector<std::string> FileResource::updateColumns() const
{
    std::vector<std::string> ret;
    if (dirtyFlag_[1])
    {
        ret.push_back(getColumnName(1));
    }
    if (dirtyFlag_[2])
    {
        ret.push_back(getColumnName(2));
    }
    if (dirtyFlag_[3])
    {
        ret.push_back(getColumnName(3));
    }
    if (dirtyFlag_[4])
    {
        ret.push_back(getColumnName(4));
    }
    if (dirtyFlag_[5])
    {
        ret.push_back(getColumnName(5));
    }
    if (dirtyFlag_[6])
    {
        ret.push_back(getColumnName(6));
    }
    if (dirtyFlag_[7])
    {
        ret.push_back(getColumnName(7));
    }
    if (dirtyFlag_[8])
    {
        ret.push_back(getColumnName(8));
    }
    if (dirtyFlag_[9])
    {
        ret.push_back(getColumnName(9));
    }
    if (dirtyFlag_[10])
    {
        ret.push_back(getColumnName(10));
    }
    if (dirtyFlag_[11])
    {
        ret.push_back(getColumnName(11));
    }
    return ret;
}

void FileResource::updateArgs(drogon::orm::internal::SqlBinder &binder) const
{
    if (dirtyFlag_[1])
    {
        if (getUserId())
        {
            binder << getValueOfUserId();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[2])
    {
        if (getNoteId())
        {
            binder << getValueOfNoteId();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[3])
    {
        if (getFileName())
        {
            binder << getValueOfFileName();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[4])
    {
        if (getFilePath())
        {
            binder << getValueOfFilePath();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[5])
    {
        if (getFileType())
        {
            binder << getValueOfFileType();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[6])
    {
        if (getFileSize())
        {
            binder << getValueOfFileSize();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[7])
    {
        if (getObjectKey())
        {
            binder << getValueOfObjectKey();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[8])
    {
        if (getUrl())
        {
            binder << getValueOfUrl();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[9])
    {
        if (getStatus())
        {
            binder << getValueOfStatus();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[10])
    {
        if (getCreatedAt())
        {
            binder << getValueOfCreatedAt();
        }
        else
        {
            binder << nullptr;
        }
    }
    if (dirtyFlag_[11])
    {
        if (getUpdatedAt())
        {
            binder << getValueOfUpdatedAt();
        }
        else
        {
            binder << nullptr;
        }
    }
}

Json::Value FileResource::toJson() const
{
    Json::Value ret;
    if (getId())
    {
        ret["id"] = static_cast<Json::Int64>(getValueOfId());
    }
    if (getUserId())
    {
        ret["user_id"] = static_cast<Json::Int64>(getValueOfUserId());
    }
    if (getNoteId())
    {
        ret["note_id"] = static_cast<Json::Int64>(getValueOfNoteId());
    }
    if (getFileName())
    {
        ret["file_name"] = getValueOfFileName();
    }
    if (getFilePath())
    {
        ret["file_path"] = getValueOfFilePath();
    }
    if (getFileType())
    {
        ret["file_type"] = getValueOfFileType();
    }
    if (getFileSize())
    {
        ret["file_size"] = static_cast<Json::Int64>(getValueOfFileSize());
    }
    if (getObjectKey())
    {
        ret["object_key"] = getValueOfObjectKey();
    }
    if (getUrl())
    {
        ret["url"] = getValueOfUrl();
    }
    if (getStatus())
    {
        ret["status"] = getValueOfStatus();
    }
    if (getCreatedAt())
    {
        ret["created_at"] = getValueOfCreatedAt().toDbStringLocal();
    }
    if (getUpdatedAt())
    {
        ret["updated_at"] = getValueOfUpdatedAt().toDbStringLocal();
    }
    return ret;
}

std::string FileResource::toString() const
{
    return toJson().toStyledString();
}
