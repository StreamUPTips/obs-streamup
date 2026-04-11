#include "http-client.hpp"
#include "debug-logger.hpp"
#include "../version.h"
#include <curl/curl.h>
#include <pthread.h>
#include <obs-module.h>

namespace StreamUP {  
namespace HttpClient {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out)
{
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

bool MakeGetRequest(const std::string& url, std::string& response)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        StreamUP::DebugLogger::LogError("HttpClient", "Failed to initialize curl");
        return false;
    }

    // Set headers for GitHub API compatibility
    struct curl_slist* headers = nullptr;
    std::string userAgent = std::string("User-Agent: StreamUP-OBS-Plugin/") + PROJECT_VERSION;
    headers = curl_slist_append(headers, userAgent.c_str());
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        StreamUP::DebugLogger::LogWarningFormat("HttpClient", "HTTP request failed: %s", curl_easy_strerror(res));
        return false;
    }

    return true;
}

struct AsyncRequestData {
    std::string url;
    std::function<void(const std::string&, const std::string&, bool)> callback;
};

void* AsyncRequestThread(void* arg)
{
    AsyncRequestData* asyncData = (AsyncRequestData*)arg;
    std::string response;
    
    bool success = MakeGetRequest(asyncData->url, response);
    
    // Call the callback with the results
    asyncData->callback(asyncData->url, response, success);
    
    // Clean up
    delete asyncData;
    return nullptr;
}

bool MakeAsyncGetRequest(const std::string& url, 
                        std::function<void(const std::string&, const std::string&, bool)> callback)
{
    AsyncRequestData* asyncData = new AsyncRequestData{url, callback};
    
    pthread_t thread;
    int result = pthread_create(&thread, nullptr, AsyncRequestThread, asyncData);
    
    if (result != 0) {
        StreamUP::DebugLogger::LogError("HttpClient", "Failed to create HTTP request thread");
        delete asyncData;
        return false;
    }
    
    // Detach thread so it cleans up automatically
    pthread_detach(thread);
    return true;
}

} // namespace HttpClient
} // namespace StreamUP
