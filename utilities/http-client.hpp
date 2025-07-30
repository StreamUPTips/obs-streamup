#ifndef STREAMUP_HTTP_CLIENT_HPP
#define STREAMUP_HTTP_CLIENT_HPP

#include <string>
#include <functional>

namespace StreamUP {
namespace HttpClient {

/**
 * @brief Structure to hold HTTP request data
 */
struct RequestData {
    std::string url;
    std::string response;
};

/**
 * @brief Make a synchronous HTTP GET request
 * @param url The URL to request
 * @param response Output parameter for the response data
 * @return bool True if the request was successful, false otherwise
 */
bool MakeGetRequest(const std::string& url, std::string& response);

/**
 * @brief Make an asynchronous HTTP GET request using pthread
 * @param url The URL to request
 * @param callback Function to call when the request completes (url, response, success)
 * @return bool True if the request was started successfully, false otherwise
 */
bool MakeAsyncGetRequest(const std::string& url, 
                        std::function<void(const std::string&, const std::string&, bool)> callback);

/**
 * @brief Internal callback for curl write operations
 * @param contents The data received from curl
 * @param size Size of individual elements
 * @param nmemb Number of elements
 * @param out Output string to append data to
 * @return size_t Number of bytes processed
 */
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out);

/**
 * @brief Internal pthread function for making API requests
 * @param arg Pointer to RequestData structure
 * @return void* Always returns nullptr
 */
void* MakeApiRequestThread(void* arg);

} // namespace HttpClient
} // namespace StreamUP

#endif // STREAMUP_HTTP_CLIENT_HPP