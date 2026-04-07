/**
 * FileController.h
 * File management controller for upload, list, delete, and status operations
 */

#pragma once

#include "../models/FileResource.h"
#include "../services/AuthService.h"
#include "../utils/MinioClient.h"
#include <drogon/HttpController.h>

using namespace drogon;

namespace calcite {
namespace api {
namespace v1 {

class FileController : public drogon::HttpController<FileController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FileController::uploadFile, "/api/file/upload", Post);
    ADD_METHOD_TO(FileController::listFiles, "/api/file/list", Get);
    ADD_METHOD_TO(FileController::deleteFile, "/api/file/delete", Post);
    ADD_METHOD_TO(FileController::getFileStatus, "/api/file/status", Get);
    ADD_METHOD_TO(FileController::getFileInfo, "/api/file/info", Get);
    METHOD_LIST_END

    /**
     * POST /api/file/upload
     * Upload a file to MinIO and create database record
     */
    void uploadFile(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * GET /api/file/list
     * Get file list with optional filtering by user_id and note_id
     */
    void listFiles(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * POST /api/file/delete
     * Delete a file from both MinIO and database
     */
    void deleteFile(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * GET /api/file/status
     * Query file upload status
     */
    void getFileStatus(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * GET /api/file/info
     * Get single file details
     */
    void getFileInfo(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

private:
    services::AuthService authService_;
    utils::MinioClient minioClient_;

    /**
     * Create standard JSON response
     */
    Json::Value createResponse(int code, const std::string &message, const Json::Value &data = Json::Value());

    /**
     * Verify JWT token and get user ID
     */
    void verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback);

    /**
     * Process file upload with async MinIO operation
     * 
     * @param fileRecord The database record for the file
     * @param fileData File data buffer
     * @param dataSize Size of data
     * @param mapper ORM mapper for database operations
     */
    void processFileUpload(
        drogon_model::calcite::FileResource fileRecord,
        std::shared_ptr<std::vector<uint8_t>> fileData,
        const std::string& contentType,
        drogon::orm::Mapper<drogon_model::calcite::FileResource>& mapper
    );

    /**
     * Update file status in database
     */
    void updateFileStatus(
        int64_t fileId,
        const std::string& status,
        const std::string& url,
        drogon::orm::Mapper<drogon_model::calcite::FileResource>& mapper,
        std::function<void(bool)> callback
    );

    /**
     * Format file size to human readable string
     */
    static std::string formatFileSize(int64_t bytes);

    /**
     * Get content type from file extension
     */
    static std::string getContentType(const std::string& fileName);
};

} // namespace v1
} // namespace api
} // namespace calcite
