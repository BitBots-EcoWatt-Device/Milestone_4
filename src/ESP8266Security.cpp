#include "ESP8266Security.h"
#include "ESP8266Config.h"
#include <SHA256.h>
#include <base64.hpp>

String ESP8266Security::calculateHMAC(const char *key, const uint32_t nonce, const String &payload)
{
    // Construct the canonical message to be signed.
    String message_to_sign = String(nonce) + "." + payload;

    SHA256 sha256;

    // Reset HMAC with the provided key
    sha256.resetHMAC(key, strlen(key));

    // Update HMAC with the message to sign
    sha256.update(message_to_sign.c_str(), message_to_sign.length());

    // Finalize HMAC calculation
    uint8_t mac_result[SHA256::HASH_SIZE];
    sha256.finalizeHMAC(key, strlen(key), mac_result, sizeof(mac_result));

    // Convert the raw binary signature into a hexadecimal string.
    String mac_hex_string = "";
    mac_hex_string.reserve(sizeof(mac_result) * 2 + 1); // Pre-allocate memory for efficiency

    for (int i = 0; i < sizeof(mac_result); i++)
    {
        char hex_buf[3]; // Buffer for 2 hex characters plus a null terminator
        sprintf(hex_buf, "%02x", mac_result[i]);
        mac_hex_string += hex_buf;
    }

    return mac_hex_string;
}

String ESP8266Security::createSecureWrapper(const DynamicJsonDocument &original_doc)
{
    // Serialize the original data payload into a string.
    String payload_str;
    serializeJson(original_doc, payload_str);

    return createSecureWrapper(payload_str);
}

String ESP8266Security::createSecureWrapper(const String serialized_payload)
{
    // Encode the payload string with Base64 for simplified confidentiality.
    // Calculate required buffer size
    unsigned int encoded_length = encode_base64_length(serialized_payload.length());
    unsigned char *encoded_buffer = new unsigned char[encoded_length + 1];

    // Encode the payload
    encode_base64((const unsigned char *)serialized_payload.c_str(), serialized_payload.length(), encoded_buffer);
    encoded_buffer[encoded_length] = '\0'; // Null terminate

    String encoded_payload = String((char *)encoded_buffer);
    delete[] encoded_buffer;

    // Get the next unique nonce. This also saves the new value to EEPROM.
    uint32_t nonce_to_use = configManager.getNextNonce();

    // Get the Pre-Shared Key from configuration.
    const char *psk = configManager.getSecurityConfig().psk;

    // Calculate the HMAC signature on the nonce and the *encoded* payload.
    String mac_signature = calculateHMAC(psk, nonce_to_use, encoded_payload);

    // Assemble the final, secure JSON wrapper.
    DynamicJsonDocument secure_doc(1024);
    secure_doc["nonce"] = nonce_to_use;
    secure_doc["payload"] = encoded_payload;
    secure_doc["mac"] = mac_signature;

    // Serialize the final wrapper to a string and return it.
    String final_secure_payload;
    serializeJson(secure_doc, final_secure_payload);

    return final_secure_payload;
}