/**
 * MinioClient.h
 * MinIO S3 client utility for file storage operations
 */

#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>

namespace calcite {
namespace utils {

/**
 * MinIO client configuration
 */
struct MinioConfig {
    std::string endpoint = "http://127.0.0.1:9000";
    std::string accessKey = "admin";
    std::string secretKey = "12345678";
    std::string bucket = "notes-files";
    std::string region = "us-east-1";  // Default S3 region
};

/**
 * Upload result structure
 */
struct UploadResult {
    bool success = false;
    std::string errorMessage;
    std::string etag;
};

/**
 * Delete result structure
 */
struct DeleteResult {
    bool success = false;
    std::string errorMessage;
};

/**
 * MinIO S3 Client
 * 
 * This client provides async operations for:
 * - Uploading files to MinIO
 * - Deleting files from MinIO
 * - Generating presigned URLs
 */
class MinioClient {
public:
    MinioClient();
    explicit MinioClient(const MinioConfig& config);
    ~MinioClient();

    /**
     * Upload file data to MinIO
     * 
     * @param objectKey The object key (path in bucket)
     * @param data File data buffer
     * @param dataSize Size of data in bytes
     * @param contentType MIME type of the file
     * @param callback Callback function with UploadResult
     */
    void uploadFile(
        const std::string& objectKey,
        const uint8_t* data,
        size_t dataSize,
        const std::string& contentType,
        std::function<void(const UploadResult&)> callback
    );

    /**
     * Upload file from local path to MinIO
     * 
     * @param objectKey The object key (path in bucket)
     * @param localFilePath Path to local file
     * @param contentType MIME type of the file
     * @param callback Callback function with UploadResult
     */
    void uploadFileFromPath(
        const std::string& objectKey,
        const std::string& localFilePath,
        const std::string& contentType,
        std::function<void(const UploadResult&)> callback
    );

    /**
     * Delete object from MinIO
     * 
     * @param objectKey The object key to delete
     * @param callback Callback function with DeleteResult
     */
    void deleteObject(
        const std::string& objectKey,
        std::function<void(const DeleteResult&)> callback
    );

    /**
     * Generate MinIO access URL
     * 
     * @param objectKey The object key
     * @return Full URL to access the object
     */
    std::string generateObjectUrl(const std::string& objectKey) const;

    /**
     * Get the configured bucket name
     */
    std::string getBucket() const { return config_.bucket; }

    /**
     * Generate object key according to rule: userId/yyyy/mm/dd/uuid.ext
     * 
     * @param userId User ID
     * @param fileName Original file name (for extension)
     * @return Generated object key
     */
    static std::string generateObjectKey(int64_t userId, const std::string& fileName);

private:
    MinioConfig config_;

    /**
     * Build S3 authorization header using AWS Signature Version 4
     */
    std::string buildAuthorizationHeader(
        const std::string& method,
        const std::string& uri,
        const std::string& contentType,
        const std::string& dateStr,
        const std::vector<uint8_t>& payloadHash
    ) const;

    /**
     * Calculate SHA256 hash
     */
    static std::vector<uint8_t> sha256(const uint8_t* data, size_t len);
    static std::vector<uint8_t> sha256(const std::string& str);
    
    /**
     * Calculate HMAC-SHA256
     */
    static std::vector<uint8_t> hmacSha256(
        const std::vector<uint8_t>& key,
        const std::string& data
    );

    /**
     * Convert bytes to hex string
     */
    static std::string toHexString(const std::vector<uint8_t>& data);

    /**
     * Get current date in YYYYMMDD format
     */
    static std::string getDateStamp();

    /**
     * Get current datetime in ISO8601 format
     */
    static std::string getAmzDate();

    /**
     * Trim leading/trailing whitespace
     */
    static std::string trim(const std::string& str);
};

} // namespace utils
} // namespace calcite
