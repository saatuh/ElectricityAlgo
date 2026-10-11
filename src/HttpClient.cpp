#include "HttpClient.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <string>

namespace {
    class CurlGlobal {
    public:
        CurlGlobal() {
            const CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);

            if (result != CURLE_OK) {
                throw std::runtime_error("curl_global_init failed: " + std::string(curl_easy_strerror(result))
                );
            }
        }

        ~CurlGlobal(){
            curl_global_cleanup();
        }
    };

    CurlGlobal curlGlobal;
}

HttpClient::HttpClient() {
    (void)curlGlobal;
}

HttpClient::~HttpClient() = default;

size_t HttpClient::writeCallback(char* data, size_t size, size_t count, void* userData) {
    const size_t totalBytes = size * count;
    auto* response = static_cast<std::string*>(userData);

    response->append(data, totalBytes);

    return totalBytes;
}

nlohmann::json HttpClient::getJson(const std::string& url) const {
    CURL* curl = curl_easy_init();

    if (curl == nullptr) {
        throw std::runtime_error("Could not create a CURL handle.");
    }

    std::string responseBody;
    long httpStatus = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,&HttpClient::writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ElectricityAlgo/1.0");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    const CURLcode result = curl_easy_perform(curl);

    if (result == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,&httpStatus);
    }

    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        throw std::runtime_error("HTTP request failed for " + url + ": " + curl_easy_strerror(result));
    }

    if (httpStatus < 200 || httpStatus >= 300){
        throw std::runtime_error("HTTP status " + std::to_string(httpStatus) + " for " + url + ". Response: " + responseBody.substr(0, 700));
    }

    try {
        return nlohmann::json::parse(responseBody);
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error( "Could not parse JSON from " + url + ": " + error.what() + ". Response: " + responseBody.substr(0, 500));
    }
}
