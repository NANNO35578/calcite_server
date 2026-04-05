#pragma once
#include <drogon/HttpClient.h>
#include <string>
#include <functional>

class EsClient {
public:
    EsClient(const std::string& host = "http://localhost:9200")
        : client_(drogon::HttpClient::newHttpClient(host)) {}

    void indexDocument(const std::string& index,
                       const std::string& id,
                       const std::string& json);

    bool indexDocumentSync(const std::string& index,
                            const std::string& id,
                            const std::string& json,
                            int timeoutMs = 5000);

    void deleteDocument(const std::string& index,
                        const std::string& id);

    bool deleteDocumentSync(const std::string& index,
                            const std::string& id,
                            int timeoutMs = 5000);

    void search(const std::string& index,
                const std::string& query,
                std::function<void(const std::string&)> callback);

    std::string searchSync(const std::string& index,
                           const std::string& query,
                           int timeoutMs = 5000);

    drogon::HttpClientPtr getClient() const;

private:
    drogon::HttpClientPtr client_;
};
