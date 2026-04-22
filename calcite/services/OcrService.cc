/**
 * OcrService.cc
 * OCR service implementation
 */

#include "OcrService.h"
#include <drogon/drogon.h>
#include <curl/curl.h>
#include <json/json.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace calcite {
namespace services {

// Base64 encoding table
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

OcrService::OcrService() {
    const char* envToken = std::getenv("CALCITE_OCR_API_TOKEN");
    if (envToken && std::strlen(envToken) > 0) {
        apiToken_ = envToken;
    } else {
        std::cerr << "[Security Warning] CALCITE_OCR_API_TOKEN not set. OCR service will fail." << std::endl;
    }
}
OcrService::~OcrService() = default;

std::string OcrService::base64Encode(const std::vector<uint8_t>& data) {
    std::string encoded;
    size_t i = 0;
    uint8_t array3[3];
    uint8_t array4[4];
    
    for (uint8_t byte : data) {
        array3[i++] = byte;
        if (i == 3) {
            array4[0] = (array3[0] & 0xfc) >> 2;
            array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
            array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
            array4[3] = array3[2] & 0x3f;
            
            for (int j = 0; j < 4; j++) {
                encoded += base64_chars[array4[j]];
            }
            i = 0;
        }
    }
    
    if (i > 0) {
        for (int j = i; j < 3; j++) {
            array3[j] = '\0';
        }
        
        array4[0] = (array3[0] & 0xfc) >> 2;
        array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
        array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
        
        for (int j = 0; j < i + 1; j++) {
            encoded += base64_chars[array4[j]];
        }
        
        while (i++ < 3) {
            encoded += '=';
        }
    }
    
    return encoded;
}

std::string OcrService::buildRequestJson(const std::string& base64Data, OcrFileType fileType) {
    Json::Value root;
    root["file"] = base64Data;
    root["fileType"] = (fileType == OcrFileType::PDF) ? 0 : 1;
    
    // Optional parameters
    root["useDocOrientationClassify"] = false;
    root["useDocUnwarping"] = false;
    root["useChartRecognition"] = false;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

OcrResult OcrService::parseResponse(const std::string& responseData) {
    OcrResult result;
    
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(responseData.c_str(), responseData.c_str() + responseData.size(), 
                       &root, &errors)) {
        result.success = false;
        result.errorMessage = "Failed to parse JSON response: " + errors;
        return result;
    }
    
    // Check if response contains error
    if (!root.isMember("result") || root["result"].isNull()) {
        result.success = false;
        result.errorMessage = "OCR API returned no result";
        return result;
    }
    
    const Json::Value& ocrResult = root["result"];
    
    // Check for layout parsing results
    if (!ocrResult.isMember("layoutParsingResults") || !ocrResult["layoutParsingResults"].isArray()) {
        result.success = false;
        result.errorMessage = "OCR API returned invalid layout parsing results";
        return result;
    }
    
    const Json::Value& parsingResults = ocrResult["layoutParsingResults"];
    result.pageCount = parsingResults.size();
    
    if (result.pageCount == 0) {
        result.success = false;
        result.errorMessage = "OCR returned no content";
        return result;
    }
    
    // Concatenate markdown from all pages
    std::stringstream markdownStream;
    for (const auto& page : parsingResults) {
        if (page.isMember("markdown") && page["markdown"].isMember("text")) {
            markdownStream << page["markdown"]["text"].asString();
            markdownStream << "\n\n"; // Add separator between pages
        }
    }
    
    result.markdownText = markdownStream.str();
    result.success = !result.markdownText.empty();
    
    if (!result.success) {
        result.errorMessage = "OCR returned empty content";
    }
    
    return result;
}

void OcrService::performOcrRequest(
    const std::string& requestJson,
    std::function<void(const OcrResult&)> callback
) {
    // Run in background thread
    drogon::app().getLoop()->queueInLoop([this, requestJson, callback]() {
        std::string apiToken = apiToken_;
        std::thread([requestJson, callback, apiToken]() {
            CURL* curl = curl_easy_init();
            if (!curl) {
                OcrResult result;
                result.success = false;
                result.errorMessage = "Failed to initialize CURL";
                drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
                return;
            }
            
            if (apiToken.empty()) {
                OcrResult result;
                result.success = false;
                result.errorMessage = "OCR API token not configured";
                drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
                curl_easy_cleanup(curl);
                return;
            }

            // Build authorization header
            std::string authHeader = "Authorization: token ";
            authHeader += apiToken;
            
            CurlResponse response;
            
            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, authHeader.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            
            curl_easy_setopt(curl, CURLOPT_URL, API_URL);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestJson.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, API_TIMEOUT);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Disable SSL verification for simplicity
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.httpCode);
            
            OcrResult result;
            if (res != CURLE_OK) {
                result.success = false;
                result.errorMessage = std::string("CURL error: ") + curl_easy_strerror(res);
            } else if (response.httpCode == 200) {
                result = parseResponse(response.data);
            } else {
                result.success = false;
                result.errorMessage = "HTTP error " + std::to_string(response.httpCode);
                if (!response.data.empty()) {
                    result.errorMessage += ": " + response.data;
                }
            }
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            drogon::app().getLoop()->queueInLoop([callback, result]() { callback(result); });
        }).detach();
    });
}

void OcrService::recognize(
    const std::vector<uint8_t>& fileData,
    OcrFileType fileType,
    std::function<void(const OcrResult&)> callback
) {
    if (fileData.empty()) {
        OcrResult result;
        result.success = false;
        result.errorMessage = "Empty file data";
        callback(result);
        return;
    }
    
    // Encode file to base64
    std::string base64Data = base64Encode(fileData);
    
    // Build request JSON
    std::string requestJson = buildRequestJson(base64Data, fileType);
    
    // Perform OCR request
    performOcrRequest(requestJson, callback);
}

bool OcrService::isSupportedFileType(const std::string& fileName) {
    size_t dotPos = fileName.rfind('.');
    if (dotPos == std::string::npos) {
        return false;
    }
    
    std::string ext = fileName.substr(dotPos + 1);
    // Convert to lowercase
    for (auto& c : ext) {
        c = std::tolower(c);
    }
    
    // Supported image formats
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp" || ext == "webp") {
        return true;
    }
    
    // Supported PDF
    if (ext == "pdf") {
        return true;
    }
    
    return false;
}

OcrFileType OcrService::getFileType(const std::string& fileName) {
    size_t dotPos = fileName.rfind('.');
    if (dotPos == std::string::npos) {
        return OcrFileType::IMAGE; // Default to image
    }
    
    std::string ext = fileName.substr(dotPos + 1);
    // Convert to lowercase
    for (auto& c : ext) {
        c = std::tolower(c);
    }
    
    if (ext == "pdf") {
        return OcrFileType::PDF;
    }
    
    return OcrFileType::IMAGE;
}

} // namespace services
} // namespace calcite
