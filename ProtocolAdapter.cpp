#include "ProtocolAdapter.h"
#include "Config.h"
#include <curl/curl.h>
#include <iostream>
#include <vector>
#include <iomanip>

// ========== CURL callback ==========
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

// ========== Post JSON ==========
bool post_json(const std::string &url, const std::string &apiKey,
               const std::string &frameHex, std::string &outFrameHex)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return false;

    std::string readBuffer;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: " + apiKey).c_str());

    std::string body = "{\"frame\":\"" + frameHex + "\"}";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK)
        return false;

    auto pos = readBuffer.find("\"frame\":\"");
    if (pos == std::string::npos)
        return false;
    pos += 9;
    auto end = readBuffer.find("\"", pos);
    if (end == std::string::npos)
        return false;
    outFrameHex = readBuffer.substr(pos, end - pos);

    return true;
}

// ========== ProtocolAdapter methods ==========
ProtocolAdapter::ProtocolAdapter()
{
    if (!initializeConfig())
    {
        std::cerr << "Error: Failed to initialize ProtocolAdapter configuration" << std::endl;
    }
}

bool ProtocolAdapter::initializeConfig()
{
    Config &config = Config::getInstance();

    // Load config if not already loaded
    if (!config.isLoaded())
    {
        if (!config.loadFromFile())
        {
            return false;
        }
    }

    // Get configuration values
    apiKey_ = config.getApiKey();
    readURL_ = config.getReadUrl();
    writeURL_ = config.getWriteUrl();

    return !apiKey_.empty() && !readURL_.empty() && !writeURL_.empty();
}

bool ProtocolAdapter::sendReadRequest(const std::string &frameHex, std::string &outFrameHex)
{
    return post_json(readURL_, apiKey_, frameHex, outFrameHex);
}

bool ProtocolAdapter::sendWriteRequest(const std::string &frameHex, std::string &outFrameHex)
{
    return post_json(writeURL_, apiKey_, frameHex, outFrameHex);
}
