#pragma once

#include <nlohmann/json.hpp>
#include <string>

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    nlohmann::json getJson(const std::string& url) const;

private:
    static size_t writeCallback(
        char* data,
        size_t size,
        size_t count,
        void* userData
    );
};
