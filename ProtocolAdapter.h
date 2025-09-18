#ifndef PROTOCOL_ADAPTER_H
#define PROTOCOL_ADAPTER_H

#include <cstdint>
#include <string>
#include <vector>

class ProtocolAdapter
{
public:
    ProtocolAdapter();

    // Send a read frame and return response hex
    bool sendReadRequest(const std::string &frameHex, std::string &outFrameHex);

    // Send a write frame and return response hex
    bool sendWriteRequest(const std::string &frameHex, std::string &outFrameHex);

private:
    // Configuration is loaded from config file
    bool initializeConfig();
    std::string apiKey_;
    std::string readURL_;
    std::string writeURL_;
};

// Internal helper (hidden from main)
bool post_json(const std::string &url, const std::string &apiKey,
               const std::string &frameHex, std::string &outFrameHex);

#endif
