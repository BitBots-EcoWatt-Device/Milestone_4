#ifndef ESP8266_SECURITY_H
#define ESP8266_SECURITY_H

#include <Arduino.h>
#include <ArduinoJson.h>

class ESP8266Security
{
public:
    static String calculateHMAC(const char *key, const uint32_t nonce, const String &payload);

    static String createSecureWrapper(const DynamicJsonDocument &original_doc);

    template <size_t N>
    static String createSecureWrapper(const StaticJsonDocument<N> &original_doc)
    {
        String payload_str;
        serializeJson(original_doc, payload_str);
        return createSecureWrapper(payload_str);
    }

private:
    static String createSecureWrapper(const String serialized_payload);
};

#endif // ESP8266_SECURITY_H
