/**
 * OcrController.cc
 * OCR controller implementation
 */

#include "OcrController.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <cstdio>

namespace calcite {
namespace api {
namespace v1 {

Json::Value OcrController::createResponse(int code, const std::string &message, const Json::Value &data) {
    Json::Value response;
    response["code"] = code;
    response["message"] = message;
    if (!data.isNull()) {
        response["data"] = data;
    } else {
        response["data"] = Json::Value(Json::objectValue);
    }
    return response;
}

void OcrController::verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback) {
    std::string token = req->getHeader("Authorization");
    if (!token.empty() && token.find("Bearer ") == 0) {
        token = token.substr(7);
    }
    if (token.empty()) {
        token = req->getParameter("token");
    }
    if (token.empty()) {
        callback(false, 0);
        return;
    }

    authService_.verifyToken(token, [callback](bool valid, int64_t userId, const std::string &) {
        callback(valid, userId);
    });
}

std::string OcrController::getContentType(const std::string& fileName) {
    size_t dotPos = fileName.rfind('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string ext = fileName.substr(dotPos + 1);
    // Convert to lowercase
    for (auto& c : ext) {
        c = std::tolower(c);
    }
    
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "bmp") return "image/bmp";
    if (ext == "webp") return "image/webp";
    if (ext == "pdf") return "application/pdf";
    
    return "application/octet-stream";
}

std::string OcrController::formatFileSize(int64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return ss.str();
}

/**
 * Sanitize text for database insertion
 * - Remove null bytes
 * - Escape control characters (except normal whitespace: \n, \r, \t)
 * - Handle invalid UTF-8 sequences
 */
static std::string sanitizeText(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    
    for (size_t i = 0; i < input.length(); ) {
        unsigned char c = input[i];
        
        // Check for UTF-8 multi-byte sequence
        if (c >= 0x80) {
            // UTF-8 lead byte
            size_t seqLen = 0;
            if ((c & 0xE0) == 0xC0) seqLen = 2;      // 2-byte sequence
            else if ((c & 0xF0) == 0xE0) seqLen = 3; // 3-byte sequence
            else if ((c & 0xF8) == 0xF0) seqLen = 4; // 4-byte sequence
            
            if (seqLen > 0 && i + seqLen <= input.length()) {
                // Check if following bytes are valid continuation bytes (10xxxxxx)
                bool valid = true;
                for (size_t j = 1; j < seqLen; j++) {
                    if ((input[i + j] & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    // Copy valid UTF-8 sequence
                    output.append(input, i, seqLen);
                    i += seqLen;
                    continue;
                }
            }
            // Invalid UTF-8, skip this byte
            i++;
            continue;
        }
        
        // Handle ASCII characters
        if (c == '\0') {
            // Remove null bytes
            i++;
        } else if (c == '\t' || c == '\n' || c == '\r') {
            // Keep normal whitespace
            output += c;
            i++;
        } else if (c < 0x20) {
            // Escape other control characters (\x01-\x1F except \t, \n, \r)
            char buf[7];
            snprintf(buf, sizeof(buf), "\\x%02x", c);
            output += buf;
            i++;
        } else {
            // Normal printable ASCII
            output += c;
            i++;
        }
    }
    
    return output;
}

void OcrController::createNoteFromOcr(
    int64_t userId,
    const std::string& fileName,
    const std::string& markdownText,
    std::function<void(int64_t, const std::string&)> callback
) {
    // Create note title with format: ocr_YYYYMMDD_HHMMSS
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);
    
    std::stringstream titleStream;
    titleStream << "ocr_"
                << std::setfill('0') << std::setw(4) << (tm_now.tm_year + 1900)
                << std::setw(2) << (tm_now.tm_mon + 1)
                << std::setw(2) << tm_now.tm_mday
                << "_"
                << std::setw(2) << tm_now.tm_hour
                << std::setw(2) << tm_now.tm_min
                << std::setw(2) << tm_now.tm_sec;
    std::string title = titleStream.str();
    
    // Sanitize markdown content before inserting to database
    std::string sanitizedContent = sanitizeText(markdownText);
    
    // Create note
    drogon_model::calcite::Note note;
    note.setUserId(userId);
    note.setTitle(title);
    note.setContent(sanitizedContent);
    // Note: summary is not set (left empty)
    
    drogon::orm::Mapper<drogon_model::calcite::Note> noteMapper(drogon::app().getDbClient("default"));
    noteMapper.insert(
        note,
        [callback](const drogon_model::calcite::Note& insertedNote) {
            callback(insertedNote.getValueOfId(), "");
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(0, std::string("Failed to create note: ") + e.base().what());
        }
    );
}

void OcrController::updateFileResource(
    int64_t fileId,
    const std::string& status,
    int64_t noteId,
    const std::string& url,
    std::function<void(bool)> callback
) {
    drogon::orm::Mapper<drogon_model::calcite::FileResource> mapper(drogon::app().getDbClient("default"));
    mapper.findByPrimaryKey(
        fileId,
        [mapper, status, noteId, url, callback](const drogon_model::calcite::FileResource& file) mutable {
            drogon_model::calcite::FileResource updatedFile = file;
            updatedFile.setStatus(status);
            if (noteId > 0) {
                updatedFile.setNoteId(noteId);
            }
            if (!url.empty()) {
                updatedFile.setUrl(url);
            }
            
            mapper.update(
                updatedFile,
                [callback](size_t) { callback(true); },
                [callback](const drogon::orm::DrogonDbException&) { callback(false); }
            );
        },
        [callback](const drogon::orm::DrogonDbException&) { callback(false); }
    );
}

void OcrController::processOcrWorkflow(
    drogon_model::calcite::FileResource fileRecord,
    std::shared_ptr<std::vector<uint8_t>> fileData,
    const std::string& contentType,
    int64_t userId
) {
    int64_t fileId = fileRecord.getValueOfId();
    std::string objectKey = fileRecord.getValueOfObjectKey();
    std::string fileName = fileRecord.getValueOfFileName();
    
    // Determine OCR file type
    services::OcrFileType ocrFileType = services::OcrService::getFileType(fileName);
    
    // Step 1: Call OCR API
    ocrService_.recognize(
        *fileData,
        ocrFileType,
        [this, fileId, objectKey, fileName, fileData, contentType, userId](const services::OcrResult& ocrResult) {
            if (!ocrResult.success) {
                // OCR failed, update status
                LOG_ERROR << "OCR failed for file_id=" << fileId << ", error=" << ocrResult.errorMessage;
                updateFileResource(fileId, "failed", 0, "", [](bool) {});
                return;
            }
            
            LOG_INFO << "OCR success for file_id=" << fileId << ", pages=" << ocrResult.pageCount;
            
            // Step 2: Create note from OCR result
            createNoteFromOcr(
                userId,
                fileName,
                ocrResult.markdownText,
                [this, fileId, objectKey, fileData, contentType, userId](int64_t noteId, const std::string& error) {
                    if (noteId <= 0) {
                        LOG_ERROR << "Failed to create note for file_id=" << fileId << ", error=" << error;
                        updateFileResource(fileId, "failed", 0, "", [](bool) {});
                        return;
                    }
                    
                    LOG_INFO << "Note created for file_id=" << fileId << ", note_id=" << noteId;
                    
                    // Step 3: Upload file to MinIO
                    std::string url = minioClient_.generateObjectUrl(objectKey);
                    
                    minioClient_.uploadFile(
                        objectKey,
                        fileData,
                        contentType,
                        [this, fileId, noteId, url](const utils::UploadResult& uploadResult) {
                            if (uploadResult.success) {
                                // Step 4: Update file_resource with success status
                                updateFileResource(
                                    fileId,
                                    "done",
                                    noteId,
                                    url,
                                    [](bool success) {
                                        if (!success) {
                                            LOG_ERROR << "Failed to update file_resource after upload";
                                        }
                                    }
                                );
                                LOG_INFO << "File upload success for file_id=" << fileId << ", note_id=" << noteId;
                            } else {
                                // Upload failed
                                LOG_ERROR << "MinIO upload failed for file_id=" << fileId 
                                          << ", error=" << uploadResult.errorMessage;
                                updateFileResource(fileId, "failed", noteId, "", [](bool) {});
                            }
                        }
                    );
                }
            );
        }
    );
}

void OcrController::recognize(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        // Parse multipart form data for file upload
        MultiPartParser parser;
        if (parser.parse(req) != 0 || parser.getFiles().empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "请求参数错误或没有上传文件"));
            callback(resp);
            return;
        }

        const auto& files = parser.getFiles();
        const auto& file = files[0];
        std::string fileName = file.getFileName();
        
        // Check if file type is supported
        if (!services::OcrService::isSupportedFileType(fileName)) {
            auto resp = HttpResponse::newHttpJsonResponse(
                createResponse(1, "不支持的文件类型，仅支持: JPG, PNG, BMP, WEBP, PDF")
            );
            callback(resp);
            return;
        }
        
        std::string contentType = getContentType(fileName);
        int64_t fileSize = file.fileLength();
        
        // Generate object key for MinIO
        std::string objectKey = utils::MinioClient::generateObjectKey(userId, fileName);

        // Save file to temporary location
        std::string tempPath = "./uploads/tmp/ocr_" + std::to_string(userId) + "_" + std::to_string(time(nullptr)) + "_" + fileName;
        file.saveAs(tempPath);
        
        // Read file data into memory
        std::ifstream fs(tempPath, std::ios::binary | std::ios::ate);
        if (!fs.is_open()) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "读取文件失败"));
            callback(resp);
            return;
        }
        std::streamsize size = fs.tellg();
        fs.seekg(0, std::ios::beg);
        auto fileData = std::make_shared<std::vector<uint8_t>>(size);
        if (!fs.read(reinterpret_cast<char*>(fileData->data()), size)) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "读取文件失败"));
            callback(resp);
            return;
        }
        fs.close();

        // Create database record first
        drogon_model::calcite::FileResource fileRecord;
        fileRecord.setUserId(userId);
        fileRecord.setFileName(fileName);
        fileRecord.setFileType(contentType);
        fileRecord.setFileSize(fileSize);
        fileRecord.setObjectKey(objectKey);
        fileRecord.setStatus("processing");

        // Insert to database
        drogon::orm::Mapper<drogon_model::calcite::FileResource> mapper(drogon::app().getDbClient("default"));
        mapper.insert(
            fileRecord,
            [this, callback, fileRecord, fileData, contentType, userId](const drogon_model::calcite::FileResource& insertedFile) {
                int64_t fileId = insertedFile.getValueOfId();
                
                // Return immediate response with file_id
                Json::Value data;
                data["file_id"] = static_cast<Json::Int64>(fileId);
                data["status"] = "processing";
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "OCR任务已提交", data));
                callback(resp);
                
                // Continue async OCR workflow
                processOcrWorkflow(insertedFile, fileData, contentType, userId);
            },
            [this, callback](const drogon::orm::DrogonDbException& e) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "创建文件记录失败: " + std::string(e.base().what())));
                callback(resp);
            }
        );
    });
}

void OcrController::getStatus(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        std::string fileIdStr = req->getParameter("file_id");
        if (fileIdStr.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "file_id参数不能为空"));
            callback(resp);
            return;
        }

        int64_t fileId;
        try {
            fileId = std::stoll(fileIdStr);
        } catch (...) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无效的file_id参数"));
            callback(resp);
            return;
        }

        drogon::orm::Mapper<drogon_model::calcite::FileResource> mapper(drogon::app().getDbClient("default"));
        mapper.findByPrimaryKey(
            fileId,
            [this, userId, callback](const drogon_model::calcite::FileResource& file) {
                if (file.getValueOfUserId() != userId) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问此文件"));
                    callback(resp);
                    return;
                }

                Json::Value data;
                data["file_id"] = static_cast<Json::Int64>(file.getValueOfId());
                data["file_name"] = file.getValueOfFileName();
                data["status"] = file.getValueOfStatus();
                data["file_size"] = static_cast<Json::Int64>(file.getValueOfFileSize());
                data["file_size_formatted"] = formatFileSize(file.getValueOfFileSize());
                
                // Include note_id if available (status is done)
                if (file.getNoteId() && file.getValueOfNoteId() > 0) {
                    data["note_id"] = static_cast<Json::Int64>(file.getValueOfNoteId());
                }
                
                // Include URL if available
                if (file.getUrl()) {
                    data["url"] = file.getValueOfUrl();
                }
                
                data["created_at"] = file.getCreatedAt() ? file.getCreatedAt()->toDbStringLocal() : "";
                data["updated_at"] = file.getUpdatedAt() ? file.getUpdatedAt()->toDbStringLocal() : "";
                
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取OCR状态成功", data));
                callback(resp);
            },
            [this, callback](const drogon::orm::DrogonDbException& e) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "文件不存在"));
                callback(resp);
            }
        );
    });
}

} // namespace v1
} // namespace api
} // namespace calcite
