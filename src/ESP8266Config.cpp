#include "ESP8266Config.h"
#include "user_config.h"

ConfigManager configManager;

ConfigManager::ConfigManager()
{
    loadDefaults();
}

bool ConfigManager::begin()
{
    EEPROM.begin(EEPROM_SIZE);
    return loadConfig();
}

bool ConfigManager::loadConfig()
{
    EEPROM.get(0, config_);

    if (!isConfigValid())
    {
        Serial.println("[CONFIG] Invalid config in EEPROM, loading defaults");
        loadDefaults();
        return false;
    }

    Serial.println("[CONFIG] Configuration loaded successfully");
    return true;
}

bool ConfigManager::saveConfig()
{
    config_.magic = CONFIG_MAGIC;
    EEPROM.put(0, config_);
    bool success = EEPROM.commit();

    if (success)
    {
        Serial.println("[CONFIG] Configuration saved successfully");
    }
    else
    {
        Serial.println("[CONFIG] Failed to save configuration");
    }

    return success;
}

void ConfigManager::loadDefaults()
{
    // WiFi defaults
    strcpy(config_.wifi.ssid, USER_WIFI_SSID);
    strcpy(config_.wifi.password, USER_WIFI_PASSWORD);
    strcpy(config_.wifi.hostname, USER_WIFI_HOSTNAME);

    // API defaults
    strcpy(config_.api.api_key, USER_API_KEY);
    strcpy(config_.api.read_url, USER_API_READ_URL);
    strcpy(config_.api.write_url, USER_API_WRITE_URL);
    config_.api.timeout_ms = USER_API_TIMEOUT_MS;

    // Device defaults
    config_.device.slave_address = USER_SLAVE_ADDRESS;
    config_.device.poll_interval_ms = USER_POLL_INTERVAL_MS;
    config_.device.upload_interval_ms = USER_UPLOAD_INTERVAL_MS;
    config_.device.buffer_size = USER_BUFFER_SIZE;

    config_.magic = CONFIG_MAGIC;
}

void ConfigManager::setWiFiConfig(const char *ssid, const char *password, const char *hostname)
{
    strncpy(config_.wifi.ssid, ssid, sizeof(config_.wifi.ssid) - 1);
    strncpy(config_.wifi.password, password, sizeof(config_.wifi.password) - 1);
    strncpy(config_.wifi.hostname, hostname, sizeof(config_.wifi.hostname) - 1);

    config_.wifi.ssid[sizeof(config_.wifi.ssid) - 1] = '\0';
    config_.wifi.password[sizeof(config_.wifi.password) - 1] = '\0';
    config_.wifi.hostname[sizeof(config_.wifi.hostname) - 1] = '\0';
}

void ConfigManager::setAPIConfig(const char *api_key, const char *read_url, const char *write_url, uint16_t timeout_ms)
{
    strncpy(config_.api.api_key, api_key, sizeof(config_.api.api_key) - 1);
    strncpy(config_.api.read_url, read_url, sizeof(config_.api.read_url) - 1);
    strncpy(config_.api.write_url, write_url, sizeof(config_.api.write_url) - 1);

    config_.api.api_key[sizeof(config_.api.api_key) - 1] = '\0';
    config_.api.read_url[sizeof(config_.api.read_url) - 1] = '\0';
    config_.api.write_url[sizeof(config_.api.write_url) - 1] = '\0';
    config_.api.timeout_ms = timeout_ms;
}

void ConfigManager::setDeviceConfig(uint8_t slave_addr, uint16_t poll_interval, uint16_t upload_interval, uint8_t buffer_size)
{
    config_.device.slave_address = slave_addr;
    config_.device.poll_interval_ms = poll_interval;
    config_.device.upload_interval_ms = upload_interval;
    config_.device.buffer_size = buffer_size;
}

bool ConfigManager::isConfigValid() const
{
    return config_.magic == CONFIG_MAGIC &&
           strlen(config_.wifi.ssid) > 0 &&
           strlen(config_.api.api_key) > 0;
}