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

    // Base64 utility functions
    static unsigned int getBase64EncodedLength(unsigned int inputLength);
    static unsigned int getBase64DecodedLength(const String &base64Data);
    static unsigned int getBase64DecodedLength(const unsigned char *base64Data, unsigned int length);
    static bool encodeBase64(const unsigned char *input, unsigned int inputLength, unsigned char *output);
    static bool decodeBase64(const String &base64Data, unsigned char *output);
    static bool decodeBase64(const unsigned char *input, unsigned int inputLength, unsigned char *output);

private:
    static String createSecureWrapper(const String serialized_payload);
};

#endif // ESP8266_SECURITY_H
