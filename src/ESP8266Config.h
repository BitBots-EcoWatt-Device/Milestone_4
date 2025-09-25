#ifndef ESP8266_CONFIG_H
#define ESP8266_CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>

struct WiFiConfig
{
    char ssid[32];
    char password[64];
    char hostname[32];
};

struct APIConfig
{
    char api_key[128];
    char read_url[128];
    char write_url[128];
    uint16_t timeout_ms;
};

struct DeviceConfig
{
    uint8_t slave_address;
    uint16_t poll_interval_ms;
    uint16_t upload_interval_ms;
    uint8_t buffer_size;
};

struct ESP8266Config
{
    WiFiConfig wifi;
    APIConfig api;
    DeviceConfig device;
    uint32_t magic; // For EEPROM validation
};

class ConfigManager
{
public:
    ConfigManager();

    bool begin();
    bool loadConfig();
    bool saveConfig();
    void loadDefaults();

    // Getters
    const WiFiConfig &getWiFiConfig() const { return config_.wifi; }
    const APIConfig &getAPIConfig() const { return config_.api; }
    const DeviceConfig &getDeviceConfig() const { return config_.device; }

    // Setters
    void setWiFiConfig(const char *ssid, const char *password, const char *hostname = "bitbots-ecoWatt");
    void setAPIConfig(const char *api_key, const char *read_url, const char *write_url, uint16_t timeout_ms = 5000);
    void setDeviceConfig(uint8_t slave_addr, uint16_t poll_interval, uint16_t upload_interval, uint8_t buffer_size);

private:
    ESP8266Config config_;
    static const uint32_t CONFIG_MAGIC = 0xBEEFCAFE;
    static const int EEPROM_SIZE = sizeof(ESP8266Config);

    bool isConfigValid() const;
};

extern ConfigManager configManager;

#endif // ESP8266_CONFIG_H