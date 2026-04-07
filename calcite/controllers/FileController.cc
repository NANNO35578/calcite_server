/**
 * FileController.cc
 * File management controller implementation
 */

#include "FileController.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace calcite {
namespace api {
namespace v1 {

Json::Value FileController::createResponse(int code, const std::string &message, const Json::Value &data) {
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

void FileController::verifyTokenAndGetUserId(const HttpRequestPtr &req, std::function<void(bool, int64_t)> callback) {
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

std::string FileController::formatFileSize(int64_t bytes) {
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

std::string FileController::getContentType(const std::string& fileName) {
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
    if (ext == "txt") return "text/plain";
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "xml") return "application/xml";
    if (ext == "zip") return "application/zip";
    if (ext == "rar") return "application/x-rar-compressed";
    if (ext == "7z") return "application/x-7z-compressed";
    if (ext == "doc") return "application/msword";
    if (ext == "docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == "xls") return "application/vnd.ms-excel";
    if (ext == "xlsx") return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    if (ext == "ppt") return "application/vnd.ms-powerpoint";
    if (ext == "pptx") return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "mp4") return "video/mp4";
    if (ext == "avi") return "video/x-msvideo";
    if (ext == "mov") return "video/quicktime";
    
    return "application/octet-stream";
}

void FileController::updateFileStatus(
    int64_t fileId,
    const std::string& status,
    const std::string& url,
    drogon::orm::Mapper<drogon_model::calcite::FileResource>& mapper,
    std::function<void(bool)> callback
) {
    mapper.findByPrimaryKey(
        fileId,
        [&mapper, status, url, callback](const drogon_model::calcite::FileResource& file) {
            drogon_model::calcite::FileResource updatedFile = file;
            updatedFile.setStatus(status);
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

void FileController::uploadFile(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        // Parse multipart form data for file upload
        MultiPartParser parser;
        if (parser.parse(req) != 0 || parser.getFiles().empty()) {
            // Try to get from JSON body for note_id
            auto json = req->getJsonObject();
            if (!json) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "请求参数错误或没有上传文件"));
                callback(resp);
                return;
            }
        }

        // Get optional note_id from parameters
        int64_t noteId = 0;
        auto noteIdParam = req->getParameter("note_id");
        if (!noteIdParam.empty()) {
            try {
                noteId = std::stoll(noteIdParam);
            } catch (...) {
                // Ignore invalid note_id
            }
        }

        const auto& files = parser.getFiles();
        if (files.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "没有上传文件"));
            callback(resp);
            return;
        }

        const auto& file = files[0];
        std::string fileName = file.getFileName();
        std::string contentType = getContentType(fileName);
        int64_t fileSize = file.fileLength();
        
        // Generate object key for MinIO
        std::string objectKey = utils::MinioClient::generateObjectKey(userId, fileName);

        // Create database record first
        drogon_model::calcite::FileResource fileRecord;
        fileRecord.setUserId(userId);
        if (noteId > 0) {
            fileRecord.setNoteId(noteId);
        }
        fileRecord.setFileName(fileName);
        fileRecord.setFileType(contentType);
        fileRecord.setFileSize(fileSize);
        fileRecord.setObjectKey(objectKey);
        fileRecord.setStatus("processing");

        // Save file to temporary location and read it back
        std::string tempPath = "./uploads/tmp/" + std::to_string(userId) + "_" + std::to_string(time(nullptr)) + "_" + fileName;
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

        // Insert to database
        drogon::orm::Mapper<drogon_model::calcite::FileResource> mapper(drogon::app().getDbClient("default"));
        mapper.insert(
            fileRecord,
            [this, callback, fileRecord, fileData, contentType, &mapper](const drogon_model::calcite::FileResource& insertedFile) mutable {
                int64_t fileId = insertedFile.getValueOfId();
                std::string objectKey = insertedFile.getValueOfObjectKey();
                // std::cout << "After Insert " << objectKey << std::endl;

                // Return immediate response with file_id
                Json::Value data;
                data["file_id"] = static_cast<Json::Int64>(fileId);
                data["status"] = "processing";
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "文件上传已提交", data));
                callback(resp);
                
                // Continue async upload to MinIO
                std::string bucket = minioClient_.getBucket();
                std::string url = minioClient_.generateObjectUrl(objectKey);
                
                minioClient_.uploadFile(
                    objectKey,
                    fileData->data(),
                    fileData->size(),
                    contentType,
                    [this, fileId, url, &mapper](const utils::UploadResult& result) mutable {
                        if (result.success) {
                            // Update status to done
                            updateFileStatus(fileId, "done", url, mapper, [](bool) {});
                            LOG_INFO << "File upload success, file_id=" << fileId;
                        } else {
                            // Update status to failed
                            updateFileStatus(fileId, "failed", "", mapper, [](bool) {});
                            LOG_ERROR << "File upload failed, file_id=" << fileId << ", error=" << result.errorMessage;
                        }
                    }
                );
            },
            [this, callback](const drogon::orm::DrogonDbException& e) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "创建文件记录失败: " + std::string(e.base().what())));
                callback(resp);
            }
        );
    });
}

void FileController::listFiles(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        // Parse filter parameters
        std::string targetUserIdStr = req->getParameter("user_id");
        std::string noteIdStr = req->getParameter("note_id");
        std::string status = req->getParameter("status");

        int64_t targetUserId = 0;
        if (!targetUserIdStr.empty()) {
            try {
                targetUserId = std::stoll(targetUserIdStr);
            } catch (...) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无效的user_id参数"));
                callback(resp);
                return;
            }
        }

        // Security check: users can only see their own files unless admin (simplified here)
        if (targetUserId > 0 && targetUserId != userId) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权访问其他用户的文件"));
            callback(resp);
            return;
        }

        // Use the authenticated user_id
        targetUserId = userId;

        int64_t noteId = -1;
        if (!noteIdStr.empty()) {
            try {
                noteId = std::stoll(noteIdStr);
            } catch (...) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无效的note_id参数"));
                callback(resp);
                return;
            }
        }

        auto dbClient = drogon::app().getDbClient("default");

        // Build query
        std::string sql = "SELECT id, user_id, note_id, file_name, file_type, file_size, "
                         "object_key, url, status, created_at, updated_at FROM file_resource "
                         "WHERE user_id = ?";
        
        if (noteId >= 0) {
            if (noteId == 0) {
                sql += " AND note_id IS NULL";
            } else {
                sql += " AND note_id = ?";
            }
        }
        
        if (!status.empty()) {
            sql += " AND status = ?";
        }
        
        sql += " ORDER BY created_at DESC";

        // Execute query with appropriate parameters
        if (noteId > 0 && !status.empty()) {
            dbClient->execSqlAsync(
                sql,
                [this, callback](const drogon::orm::Result& result) {
                    Json::Value data(Json::arrayValue);
                    for (const auto& row : result) {
                        Json::Value fileJson;
                        fileJson["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                        fileJson["user_id"] = static_cast<Json::Int64>(row["user_id"].as<int64_t>());
                        fileJson["note_id"] = row["note_id"].isNull() ? 0 : static_cast<Json::Int64>(row["note_id"].as<int64_t>());
                        fileJson["file_name"] = row["file_name"].as<std::string>();
                        fileJson["file_type"] = row["file_type"].isNull() ? "" : row["file_type"].as<std::string>();
                        fileJson["file_size"] = row["file_size"].isNull() ? 0 : static_cast<Json::Int64>(row["file_size"].as<int64_t>());
                        fileJson["file_size_formatted"] = formatFileSize(fileJson["file_size"].asInt64());
                        fileJson["object_key"] = row["object_key"].as<std::string>();
                        fileJson["url"] = row["url"].isNull() ? "" : row["url"].as<std::string>();
                        fileJson["status"] = row["status"].isNull() ? "" : row["status"].as<std::string>();
                        fileJson["created_at"] = row["created_at"].as<std::string>();
                        fileJson["updated_at"] = row["updated_at"].as<std::string>();
                        data.append(fileJson);
                    }
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取文件列表成功", data));
                    callback(resp);
                },
                [callback, this](const drogon::orm::DrogonDbException& e) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取文件列表失败: " + std::string(e.base().what())));
                    callback(resp);
                },
                targetUserId, noteId, status
            );
        } else if (noteId > 0) {
            dbClient->execSqlAsync(
                sql,
                [this, callback](const drogon::orm::Result& result) {
                    Json::Value data(Json::arrayValue);
                    for (const auto& row : result) {
                        Json::Value fileJson;
                        fileJson["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                        fileJson["user_id"] = static_cast<Json::Int64>(row["user_id"].as<int64_t>());
                        fileJson["note_id"] = row["note_id"].isNull() ? 0 : static_cast<Json::Int64>(row["note_id"].as<int64_t>());
                        fileJson["file_name"] = row["file_name"].as<std::string>();
                        fileJson["file_type"] = row["file_type"].isNull() ? "" : row["file_type"].as<std::string>();
                        fileJson["file_size"] = row["file_size"].isNull() ? 0 : static_cast<Json::Int64>(row["file_size"].as<int64_t>());
                        fileJson["file_size_formatted"] = formatFileSize(fileJson["file_size"].asInt64());
                        fileJson["object_key"] = row["object_key"].as<std::string>();
                        fileJson["url"] = row["url"].isNull() ? "" : row["url"].as<std::string>();
                        fileJson["status"] = row["status"].isNull() ? "" : row["status"].as<std::string>();
                        fileJson["created_at"] = row["created_at"].as<std::string>();
                        fileJson["updated_at"] = row["updated_at"].as<std::string>();
                        data.append(fileJson);
                    }
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取文件列表成功", data));
                    callback(resp);
                },
                [callback, this](const drogon::orm::DrogonDbException& e) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取文件列表失败: " + std::string(e.base().what())));
                    callback(resp);
                },
                targetUserId, noteId
            );
        } else if (!status.empty()) {
            dbClient->execSqlAsync(
                sql,
                [this, callback](const drogon::orm::Result& result) {
                    Json::Value data(Json::arrayValue);
                    for (const auto& row : result) {
                        Json::Value fileJson;
                        fileJson["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                        fileJson["user_id"] = static_cast<Json::Int64>(row["user_id"].as<int64_t>());
                        fileJson["note_id"] = row["note_id"].isNull() ? 0 : static_cast<Json::Int64>(row["note_id"].as<int64_t>());
                        fileJson["file_name"] = row["file_name"].as<std::string>();
                        fileJson["file_type"] = row["file_type"].isNull() ? "" : row["file_type"].as<std::string>();
                        fileJson["file_size"] = row["file_size"].isNull() ? 0 : static_cast<Json::Int64>(row["file_size"].as<int64_t>());
                        fileJson["file_size_formatted"] = formatFileSize(fileJson["file_size"].asInt64());
                        fileJson["object_key"] = row["object_key"].as<std::string>();
                        fileJson["url"] = row["url"].isNull() ? "" : row["url"].as<std::string>();
                        fileJson["status"] = row["status"].isNull() ? "" : row["status"].as<std::string>();
                        fileJson["created_at"] = row["created_at"].as<std::string>();
                        fileJson["updated_at"] = row["updated_at"].as<std::string>();
                        data.append(fileJson);
                    }
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取文件列表成功", data));
                    callback(resp);
                },
                [callback, this](const drogon::orm::DrogonDbException& e) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取文件列表失败: " + std::string(e.base().what())));
                    callback(resp);
                },
                targetUserId, status
            );
        } else {
            dbClient->execSqlAsync(
                sql,
                [this, callback](const drogon::orm::Result& result) {
                    Json::Value data(Json::arrayValue);
                    for (const auto& row : result) {
                        Json::Value fileJson;
                        fileJson["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                        fileJson["user_id"] = static_cast<Json::Int64>(row["user_id"].as<int64_t>());
                        fileJson["note_id"] = row["note_id"].isNull() ? 0 : static_cast<Json::Int64>(row["note_id"].as<int64_t>());
                        fileJson["file_name"] = row["file_name"].as<std::string>();
                        fileJson["file_type"] = row["file_type"].isNull() ? "" : row["file_type"].as<std::string>();
                        fileJson["file_size"] = row["file_size"].isNull() ? 0 : static_cast<Json::Int64>(row["file_size"].as<int64_t>());
                        fileJson["file_size_formatted"] = formatFileSize(fileJson["file_size"].asInt64());
                        fileJson["object_key"] = row["object_key"].as<std::string>();
                        fileJson["url"] = row["url"].isNull() ? "" : row["url"].as<std::string>();
                        fileJson["status"] = row["status"].isNull() ? "" : row["status"].as<std::string>();
                        fileJson["created_at"] = row["created_at"].as<std::string>();
                        fileJson["updated_at"] = row["updated_at"].as<std::string>();
                        data.append(fileJson);
                    }
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取文件列表成功", data));
                    callback(resp);
                },
                [callback, this](const drogon::orm::DrogonDbException& e) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "获取文件列表失败: " + std::string(e.base().what())));
                    callback(resp);
                },
                targetUserId
            );
        }
    });
}

void FileController::deleteFile(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    verifyTokenAndGetUserId(req, [this, req, callback](bool valid, int64_t userId) {
        if (!valid) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "Token无效或已过期"));
            callback(resp);
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "请求参数错误"));
            callback(resp);
            return;
        }

        int64_t fileId = json->get("file_id", 0).asInt64();
        if (fileId <= 0) {
            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "文件ID无效"));
            callback(resp);
            return;
        }

        // Find the file record
        drogon::orm::Mapper<drogon_model::calcite::FileResource> mapper(drogon::app().getDbClient("default"));
        mapper.findByPrimaryKey(
            fileId,
            [this, userId, fileId, callback, &mapper](const drogon_model::calcite::FileResource& file) {
                if (file.getValueOfUserId() != userId) {
                    auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "无权删除此文件"));
                    callback(resp);
                    return;
                }

                std::string objectKey = file.getValueOfObjectKey();
                
                // Delete from MinIO first
                minioClient_.deleteObject(objectKey, [this, fileId, &mapper, callback](const utils::DeleteResult& result) {
                    // Even if MinIO delete fails, we still delete from database
                    // This ensures the record is cleaned up
                    
                    mapper.deleteByPrimaryKey(
                        fileId,
                        [this, callback, result](size_t) {
                            Json::Value data;
                            data["minio_deleted"] = result.success;
                            if (!result.success) {
                                data["minio_error"] = result.errorMessage;
                            }
                            auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "删除文件成功", data));
                            callback(resp);
                        },
                        [this, callback](const drogon::orm::DrogonDbException& e) {
                            auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "删除数据库记录失败: " + std::string(e.base().what())));
                            callback(resp);
                        }
                    );
                });
            },
            [this, callback](const drogon::orm::DrogonDbException& e) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "文件不存在"));
                callback(resp);
            }
        );
    });
}

void FileController::getFileStatus(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
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
                if (file.getUrl()) {
                    data["url"] = file.getValueOfUrl();
                }
                if (file.getFileSize()) {
                    data["file_size"] = static_cast<Json::Int64>(file.getValueOfFileSize());
                }
                
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取文件状态成功", data));
                callback(resp);
            },
            [this, callback](const drogon::orm::DrogonDbException& e) {
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(1, "文件不存在"));
                callback(resp);
            }
        );
    });
}

void FileController::getFileInfo(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
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
                data["id"] = static_cast<Json::Int64>(file.getValueOfId());
                data["user_id"] = static_cast<Json::Int64>(file.getValueOfUserId());
                data["note_id"] = file.getNoteId() ? static_cast<Json::Int64>(file.getValueOfNoteId()) : 0;
                data["file_name"] = file.getValueOfFileName();
                data["file_type"] = file.getFileType() ? file.getValueOfFileType() : "";
                data["file_size"] = file.getFileSize() ? static_cast<Json::Int64>(file.getValueOfFileSize()) : 0;
                data["file_size_formatted"] = formatFileSize(data["file_size"].asInt64());
                data["object_key"] = file.getValueOfObjectKey();
                data["url"] = file.getUrl() ? file.getValueOfUrl() : "";
                data["status"] = file.getStatus() ? file.getValueOfStatus() : "";
                data["created_at"] = file.getCreatedAt() ? file.getCreatedAt()->toDbStringLocal() : "";
                data["updated_at"] = file.getUpdatedAt() ? file.getUpdatedAt()->toDbStringLocal() : "";
                
                auto resp = HttpResponse::newHttpJsonResponse(createResponse(0, "获取文件详情成功", data));
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
