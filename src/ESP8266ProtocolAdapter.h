#ifndef ESP8266_PROTOCOL_ADAPTER_H
#define ESP8266_PROTOCOL_ADAPTER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "ESP8266Config.h"

class ESP8266ProtocolAdapter
{
public:
    ESP8266ProtocolAdapter();

    bool begin();
    bool sendReadRequest(const String &frameHex, String &outFrameHex);
    bool sendWriteRequest(const String &frameHex, String &outFrameHex);

    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }

private:
    HTTPClient httpClient_;
    WiFiClient wifiClient_;

    bool postJSON(const String &url, const String &frameHex, String &outFrameHex);
    bool connectWiFi();
    void setupHTTPHeaders(HTTPClient &client);

    unsigned long lastConnectionCheck_;
    static const unsigned long CONNECTION_CHECK_INTERVAL = 30000; // 30 seconds
};

#endif // ESP8266_PROTOCOL_ADAPTER_H