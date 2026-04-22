/**
 * OcrService.h
 * OCR service for calling third-party OCR API and processing results
 */

#pragma once

#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace calcite {
namespace services {

/**
 * OCR result structure
 */
struct OcrResult {
    bool success = false;
    std::string errorMessage;
    std::string markdownText;
    int pageCount = 0;
};

/**
 * OCR file type
 */
enum class OcrFileType {
    PDF = 0,
    IMAGE = 1
};

/**
 * OCR Service
 * 
 * This service provides:
 * - Calling third-party OCR API with base64 encoded file
 * - Parsing OCR response to extract markdown text
 */
class OcrService {
public:
    OcrService();
    ~OcrService();

    /**
     * Recognize file content using OCR
     * 
     * @param fileData File data buffer
     * @param dataSize Size of data in bytes
     * @param fileType Type of file (PDF or IMAGE)
     * @param callback Callback function with OcrResult
     */
    void recognize(
        const std::vector<uint8_t>& fileData,
        OcrFileType fileType,
        std::function<void(const OcrResult&)> callback
    );

    /**
     * Check if file extension is supported for OCR
     * 
     * @param fileName File name to check
     * @return true if supported
     */
    static bool isSupportedFileType(const std::string& fileName);

    /**
     * Determine OCR file type from file name
     * 
     * @param fileName File name
     * @return OcrFileType
     */
    static OcrFileType getFileType(const std::string& fileName);

private:
    // OCR API configuration
    static constexpr const char* API_URL = "https://jf3187r8t1fdu6r1.aistudio-app.com/layout-parsing";

    // Timeout for OCR API call (in seconds)
    static constexpr int API_TIMEOUT = 300;

    std::string apiToken_;

    /**
     * Encode file data to base64
     */
    static std::string base64Encode(const std::vector<uint8_t>& data);

    /**
     * Build OCR API request JSON
     */
    static std::string buildRequestJson(const std::string& base64Data, OcrFileType fileType);

    /**
     * Parse OCR API response
     */
    static OcrResult parseResponse(const std::string& responseData);

    /**
     * Perform HTTP POST request to OCR API
     */
    void performOcrRequest(
        const std::string& requestJson,
        std::function<void(const OcrResult&)> callback
    );
};

} // namespace services
} // namespace calcite
