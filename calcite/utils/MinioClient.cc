/**
 * MinioClient.cc
 * MinIO S3 client implementation
 */

#include "MinioClient.h"
#include <drogon/drogon.h>
#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <iomanip>

namespace calcite {
namespace utils {

// Response structure for curl operations
struct CurlResponse {
    std::string data;
    long httpCode = 0;
};

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    CurlResponse* response = static_cast<CurlResponse*>(userp);
    size_t totalSize = size * nmemb;
    response->data.append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Callback for reading file data
static size_t readCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    std::istream* stream = static_cast<std::istream*>(userdata);
    stream->read(static_cast<char*>(ptr), size * nmemb);
    return stream->gcount();
}

MinioClient::MinioClient() {
    const char* envAccessKey = std::getenv("CALCITE_MINIO_ACCESS_KEY");
    const char* envSecretKey = std::getenv("CALCITE_MINIO_SECRET_KEY");
    if (envAccessKey && std::strlen(envAccessKey) > 0) {
        config_.accessKey = envAccessKey;
    }
    if (envSecretKey && std::strlen(envSecretKey) > 0) {
        config_.secretKey = envSecretKey;
    }
}

MinioClient::MinioClient(const MinioConfig& config) : config_(config) {}

MinioClient::~MinioClient() = default;

std::string MinioClient::generateObjectKey(int64_t userId, const std::string& fileName) {
    // Generate UUID-like unique identifier
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::hex << time_t_now << ms.count();
    std::string uuid = ss.str();
    
    // Extract file extension
    std::string ext;
    size_t dotPos = fileName.rfind('.');
    if (dotPos != std::string::npos) {
        ext = fileName.substr(dotPos);
    }
    
    // Get current time components
    std::tm tm_now = *std::localtime(&time_t_now);
    std::stringstream path;
    path << userId << "/"
         << std::setfill('0') << std::setw(4) << (tm_now.tm_year + 1900) << "/"
         << std::setfill('0') << std::setw(2) << (tm_now.tm_mon + 1) << "/"
         << std::setfill('0') << std::setw(2) << tm_now.tm_mday << "/"
         << uuid << ext;
    
    return path.str();
}

std::vector<uint8_t> MinioClient::sha256(const uint8_t* data, size_t len) {
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, hash.data(), nullptr);
    EVP_MD_CTX_free(ctx);
    return hash;
}

std::vector<uint8_t> MinioClient::sha256(const std::string& str) {
    return sha256(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
}

std::vector<uint8_t> MinioClient::hmacSha256(const std::vector<uint8_t>& key, const std::string& data) {
    unsigned int len;
    std::vector<uint8_t> result(EVP_MAX_MD_SIZE);
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         result.data(), &len);
    result.resize(len);
    return result;
}

std::string MinioClient::toHexString(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::string MinioClient::getDateStamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::gmtime(&time_t_now);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << (tm_now.tm_year + 1900)
       << std::setw(2) << (tm_now.tm_mon + 1)
       << std::setw(2) << tm_now.tm_mday;
    return ss.str();
}

std::string MinioClient::getAmzDate() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::gmtime(&time_t_now);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << (tm_now.tm_year + 1900)
       << std::setw(2) << (tm_now.tm_mon + 1)
       << std::setw(2) << tm_now.tm_mday << "T"
       << std::setw(2) << tm_now.tm_hour
       << std::setw(2) << tm_now.tm_min
       << std::setw(2) << tm_now.tm_sec << "Z";
    return ss.str();
}

std::string MinioClient::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string MinioClient::buildAuthorizationHeader(
    const std::string& method,
    const std::string& uri,
    const std::string& contentType,
    const std::string& amzDate,
    const std::vector<uint8_t>& payloadHash
) const {
    std::string dateStamp = amzDate.substr(0, 8);
    std::string credentialScope = dateStamp + "/" + config_.region + "/s3/aws4_request";
    
    // Create canonical request
    std::stringstream canonicalRequest;
    canonicalRequest << method << "\n";
    canonicalRequest << uri << "\n";
    canonicalRequest << "\n";  // Query string
    canonicalRequest << "host:" << config_.endpoint.substr(config_.endpoint.find("://") + 3) << "\n";
    canonicalRequest << "x-amz-content-sha256:" << toHexString(payloadHash) << "\n";
    canonicalRequest << "x-amz-date:" << amzDate << "\n";
    canonicalRequest << "\n";
    canonicalRequest << "host;x-amz-content-sha256;x-amz-date\n";
    canonicalRequest << toHexString(payloadHash);
    
    std::vector<uint8_t> canonicalHash = sha256(canonicalRequest.str());
    
    // Create string to sign
    std::stringstream stringToSign;
    stringToSign << "AWS4-HMAC-SHA256\n";
    stringToSign << amzDate << "\n";
    stringToSign << credentialScope << "\n";
    stringToSign << toHexString(canonicalHash);
    
    // Calculate signature
    std::string kSecret = "AWS4" + config_.secretKey;
    std::vector<uint8_t> kDate = hmacSha256(
        std::vector<uint8_t>(kSecret.begin(), kSecret.end()), dateStamp);
    std::vector<uint8_t> kRegion = hmacSha256(kDate, config_.region);
    std::vector<uint8_t> kService = hmacSha256(kRegion, "s3");
    std::vector<uint8_t> kSigning = hmacSha256(kService, "aws4_request");
    std::vector<uint8_t> signature = hmacSha256(kSigning, stringToSign.str());
    
    // Build authorization header
    std::stringstream authHeader;
    authHeader << "AWS4-HMAC-SHA256 ";
    authHeader << "Credential=" << config_.accessKey << "/" << credentialScope << ", ";
    authHeader << "SignedHeaders=host;x-amz-content-sha256;x-amz-date, ";
    authHeader << "Signature=" << toHexString(signature);
    
    return authHeader.str();
}

void MinioClient::uploadFile(
    const std::string& objectKey,
    std::shared_ptr<std::vector<uint8_t>> fileData,
    const std::string& contentType,
    std::function<void(const UploadResult&)> callback
) {
    // Run in background thread to avoid blocking
    // Capture fileData shared_ptr to extend memory lifetime for async operations
    drogon::app().getLoop()->queueInLoop([this, objectKey, fileData, contentType, callback]() {
        std::vector<uint8_t> dataCopy(fileData->data(), fileData->data() + fileData->size());
        
        std::thread([this, objectKey, dataCopy, contentType, callback]() {
            CURL* curl = curl_easy_init();
            if (!curl) {
                UploadResult result;
                result.success = false;
                result.errorMessage = "Failed to initialize CURL";
                drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
                return;
            }
            
            std::string amzDate = getAmzDate();
            std::vector<uint8_t> payloadHash = sha256(dataCopy.data(), dataCopy.size());
            std::string uri = "/" + config_.bucket + "/" + objectKey;
            std::string authHeader = buildAuthorizationHeader(
                "PUT", uri, contentType, amzDate, payloadHash);
            
            std::string url = config_.endpoint + uri;
            CurlResponse response;
            
            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: " + authHeader).c_str());
            headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
            headers = curl_slist_append(headers, ("x-amz-date: " + amzDate).c_str());
            headers = curl_slist_append(headers, ("x-amz-content-sha256: " + toHexString(payloadHash)).c_str());
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, dataCopy.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, dataCopy.size());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.httpCode);
            
            UploadResult result;
            if (res != CURLE_OK) {
                result.success = false;
                result.errorMessage = std::string("CURL error: ") + curl_easy_strerror(res);
            } else if (response.httpCode >= 200 && response.httpCode < 300) {
                result.success = true;
                // Extract ETag from response
                char* etag = nullptr;
                curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &etag);
                curl_slist* recvHeaders = nullptr;
                curl_easy_getinfo(curl, CURLINFO_PRIVATE, &recvHeaders);
            } else {
                result.success = false;
                result.errorMessage = "HTTP error " + std::to_string(response.httpCode) + ": " + response.data;
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
        }).detach();
    });
}

void MinioClient::uploadFileFromPath(
    const std::string& objectKey,
    const std::string& localFilePath,
    const std::string& contentType,
    std::function<void(const UploadResult&)> callback
) {
    drogon::app().getLoop()->queueInLoop([this, objectKey, localFilePath, contentType, callback]() {
        std::thread([this, objectKey, localFilePath, contentType, callback]() {
            // Read file into memory
            std::ifstream file(localFilePath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                UploadResult result;
                result.success = false;
                result.errorMessage = "Failed to open file: " + localFilePath;
                drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
                return;
            }
            
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            std::vector<uint8_t> buffer(size);
            if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
                UploadResult result;
                result.success = false;
                result.errorMessage = "Failed to read file: " + localFilePath;
                drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
                return;
            }
            file.close();
            
            CURL* curl = curl_easy_init();
            if (!curl) {
                UploadResult result;
                result.success = false;
                result.errorMessage = "Failed to initialize CURL";
                drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
                return;
            }
            
            std::string amzDate = getAmzDate();
            std::vector<uint8_t> payloadHash = sha256(buffer.data(), buffer.size());
            std::string uri = "/" + config_.bucket + "/" + objectKey;
            std::string authHeader = buildAuthorizationHeader(
                "PUT", uri, contentType, amzDate, payloadHash);
            
            std::string url = config_.endpoint + uri;
            CurlResponse response;
            
            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: " + authHeader).c_str());
            headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
            headers = curl_slist_append(headers, ("x-amz-date: " + amzDate).c_str());
            headers = curl_slist_append(headers, ("x-amz-content-sha256: " + toHexString(payloadHash)).c_str());
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buffer.size());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.httpCode);
            
            UploadResult result;
            if (res != CURLE_OK) {
                result.success = false;
                result.errorMessage = std::string("CURL error: ") + curl_easy_strerror(res);
            } else if (response.httpCode >= 200 && response.httpCode < 300) {
                result.success = true;
            } else {
                result.success = false;
                result.errorMessage = "HTTP error " + std::to_string(response.httpCode) + ": " + response.data;
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
        }).detach();
    });
}

void MinioClient::deleteObject(
    const std::string& objectKey,
    std::function<void(const DeleteResult&)> callback
) {
    drogon::app().getLoop()->queueInLoop([this, objectKey, callback]() {
        std::thread([this, objectKey, callback]() {
            CURL* curl = curl_easy_init();
            if (!curl) {
                DeleteResult result;
                result.success = false;
                result.errorMessage = "Failed to initialize CURL";
                drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
                return;
            }
            
            std::string amzDate = getAmzDate();
            std::vector<uint8_t> emptyHash = sha256("");
            std::string uri = "/" + config_.bucket + "/" + objectKey;
            std::string authHeader = buildAuthorizationHeader(
                "DELETE", uri, "", amzDate, emptyHash);
            
            std::string url = config_.endpoint + uri;
            CurlResponse response;
            
            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: " + authHeader).c_str());
            headers = curl_slist_append(headers, ("x-amz-date: " + amzDate).c_str());
            headers = curl_slist_append(headers, ("x-amz-content-sha256: " + toHexString(emptyHash)).c_str());
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.httpCode);
            
            DeleteResult result;
            if (res != CURLE_OK) {
                result.success = false;
                result.errorMessage = std::string("CURL error: ") + curl_easy_strerror(res);
            } else if (response.httpCode == 204 || response.httpCode == 200) {
                result.success = true;
            } else {
                result.success = false;
                result.errorMessage = "HTTP error " + std::to_string(response.httpCode) + ": " + response.data;
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
        }).detach();
    });
}

std::string MinioClient::generateObjectUrl(const std::string& objectKey) const {
    return config_.endpoint + "/" + config_.bucket + "/" + objectKey;
}

} // namespace utils
} // namespace calcite
