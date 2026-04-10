/**
 * OcrController.h
 * OCR controller for image/PDF to text conversion
 */

#pragma once

#include "../models/FileResource.h"
#include "../models/Note.h"
#include "../services/AuthService.h"
#include "../services/OcrService.h"
#include "../utils/MinioClient.h"
#include "../utils/EsClient.h"
#include <drogon/HttpController.h>

using namespace drogon;

namespace calcite {
namespace api {
namespace v1 {

class OcrController : public drogon::HttpController<OcrController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(OcrController::recognize, "/api/ocr/recognize", Post);
    ADD_METHOD_TO(OcrController::getStatus, "/api/ocr/status", Get);
    METHOD_LIST_END

    /**
     * POST /api/ocr/recognize
     * Upload a file for OCR recognition and create a note from the result
     * 
     * Process:
     * 1. Receive file via multipart/form-data
     * 2. Insert file_resource record with status = processing
     * 3. Call OCR API to recognize text
     * 4. Create a note from OCR result
     * 5. Upload original file to MinIO
     * 6. Update file_resource with note_id, object_key, url, status = done
     */
    void recognize(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * GET /api/ocr/status
     * Query OCR processing status
     * 
     * Returns file processing status. If done, also returns note_id
     */
    void getStatus(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);

private:
    services::AuthService authService_;
    services::OcrService ocrService_;
    utils::MinioClient minioClient_;
  utils::EsClient esClient_;

    /**
     * Create standard JSON response
     */
    Json::Value createResponse(int code, const std::string &message, const Json::Value &data = Json::Value());

    /**
     * Verify JWT token and get user ID
     */
    void verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback);

    /**
     * Get content type from file extension
     */
    static std::string getContentType(const std::string& fileName);

    /**
     * Format file size to human readable string
     */
    static std::string formatFileSize(int64_t bytes);

    /**
     * Process OCR workflow asynchronously
     * 
     * @param fileRecord The file resource record
     * @param fileData File data buffer
     * @param contentType Content type of the file
     */
    void processOcrWorkflow(
        drogon_model::calcite::FileResource fileRecord,
        std::shared_ptr<std::vector<uint8_t>> fileData,
        const std::string& contentType,
        int64_t userId
    );

    /**
     * Create note from OCR result
     */
    void createNoteFromOcr(
        int64_t userId,
        const std::string& fileName,
        const std::string& markdownText,
        std::function<void(int64_t, const std::string&)> callback
    );

    /**
     * Update file resource status
     */
    void updateFileResource(
        int64_t fileId,
        const std::string& status,
        int64_t noteId,
        const std::string& url,
        std::function<void(bool)> callback
    );
  
  /**
   * 异步索引笔记到ES
   */
  void indexNoteToES(int64_t noteId, int64_t userId, const drogon_model::calcite::Note& note, 
                     const std::vector<std::string>& tags);
};

} // namespace v1
} // namespace api
} // namespace calcite
