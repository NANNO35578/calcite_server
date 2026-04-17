/**
 * DsService.h
 * LLM-based tag recommendation service using DeepSeek API
 */

#pragma once

#include <string>
#include <vector>
#include <functional>

namespace calcite {
namespace services {

/**
 * Tag recommendation result structure
 */
struct TagRecommendationResult {
    bool success = false;
    std::string errorMessage;
    std::vector<std::string> tags;
};

/**
 * DsService (DeepSeek Service)
 *
 * Provides LLM-powered tag recommendation by calling DeepSeek API.
 */
class DsService {
public:
    DsService();
    ~DsService();

    /**
     * Recommend 5 tags for a given note content.
     *
     * @param noteContent The note text to analyze.
     * @param callback    Callback with TagRecommendationResult.
     */
    void recommendTags(
        const std::string& noteContent,
        std::function<void(const TagRecommendationResult&)> callback
    );

private:
    // DeepSeek API configuration
    static constexpr const char* API_URL =
        "https://api.deepseek.com/chat/completions";
    static constexpr const char* API_MODEL = "deepseek-chat";
    static constexpr int API_TIMEOUT = 60; // seconds
    static constexpr const char* API_TOKEN = "sk-34135fb42fda40059f13e792ec111a4f";

    /**
     * Build the tag-recommendation prompt.
     */
    static std::string buildPrompt(const std::string& noteContent);

    /**
     * Build the DeepSeek API request JSON.
     */
    static std::string buildRequestJson(const std::string& prompt);

    /**
     * Parse DeepSeek API response and extract comma-separated tags.
     */
    static TagRecommendationResult parseResponse(const std::string& responseData);

    /**
     * Perform HTTP POST request to DeepSeek API.
     */
    void performLlmRequest(
        const std::string& requestJson,
        std::function<void(const TagRecommendationResult&)> callback
    );
};

} // namespace services
} // namespace calcite
