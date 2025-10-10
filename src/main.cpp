/*
 * BitBots EcoWatt ESP8266 Firmware
 *
 * Remote Configuration Implementation:
 * This firmware implements the new Remote Configuration Message Format Specification.
 * The device sends periodic configuration requests to the cloud and receives updates.
 *
 * Device → Cloud Configuration Request:
 * {
 *   "device_id": "EcoWatt001",
 *   "status": "ready"
 * }
 *
 * Cloud → Device Configuration Update:
 * {
 *   "config_update": {
 *     "sampling_interval": 5,
 *     "registers": ["voltage", "current", "frequency"]
 *   }
 * }
 *
 * Device → Cloud Acknowledgment:
 * {
 *   "config_ack": {
 *     "accepted": ["sampling_interval", "registers"],
 *     "rejected": [],
 *     "unchanged": []
 *   }
 * }
 *
 * Command Execution Implementation:
 * This firmware also implements command execution flow for write operations.
 *
 * Cloud → Device Command:
 * {
 *   "command": {
 *     "action": "write_register",
 *     "target_register": "export_power_percent",
 *     "value": 75
 *   }
 * }
 *
 * Device → Cloud Result:
 * {
 *   "command_result": {
 *     "status": "success",
 *     "executed_at": "2025-10-10T14:12:00Z"
 *   }
 * }
 *
 * Configuration requests are sent every 5 minutes (configurable).
 * All configuration changes take effect after the next upload cycle without reboot.
 * Commands are executed immediately and results reported on next upload.
 * Changes are persisted to EEPROM to survive power cycles.
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>

#include "ESP8266Config.h"
#include "ESP8266Inverter.h"
#include "ESP8266DataTypes.h"
#include "ESP8266PollingConfig.h"
#include "ESP8266Compression.h"

// Global objects
ESP8266Inverter inverter;
ESP8266DataBuffer dataBuffer(10); // Smaller buffer for ESP8266
ESP8266PollingConfig pollingConfig;

// Command execution structures
struct PendingCommand
{
    String action;
    String target_register;
    int value;
    unsigned long received_at;
    bool valid;
};

struct CommandResult
{
    String status;
    String executed_at;
    String error_message;
    bool has_result;
};

// Command execution state
PendingCommand pendingCommand = {"", "", 0, 0, false};
CommandResult lastCommandResult = {"", "", "", false};

// Timers
Ticker pollTicker;
Ticker uploadTicker;
Ticker configRequestTicker;

// Status variables
unsigned long startTime;
bool systemInitialized = false;
bool pendingConfigurationUpdate = false;

// Function prototypes
void setup();
void loop();
bool initializeSystem();
void pollSensors();
void uploadData();
void requestConfigUpdate();
void executeCommand();
void setupPollingConfig();
void applyNewConfiguration();
void printSystemStatus();
void handleSerialCommands();
bool uploadToServer(const std::vector<Sample> &samples);
bool sendConfigRequest();
bool executeWriteRegisterCommand(const String &register_name, int value, CommandResult &result);

// Simple CRC32 (polynomial 0xEDB88320) for MAC stub
static uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
        {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

// Flags set from timer callbacks (ISR context) to defer work to loop()
volatile bool pollPending = false;
volatile bool uploadPending = false;
volatile bool configRequestPending = false;

void IRAM_ATTR onPollTimer()
{
    pollPending = true;
}

void IRAM_ATTR onUploadTimer()
{
    uploadPending = true;
}

void IRAM_ATTR onConfigRequestTimer()
{
    configRequestPending = true;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==================================");
    Serial.println("    BitBots EcoWatt ESP8266");
    Serial.println("==================================");

    startTime = millis();
    systemInitialized = initializeSystem();

    if (systemInitialized)
    {
        setupPollingConfig();

        // Start polling and upload timers
        const DeviceConfig &deviceConfig = configManager.getDeviceConfig();
        // Timer callbacks must be ISR-safe: set a flag and perform heavy work in loop()
        pollTicker.attach_ms(deviceConfig.poll_interval_ms, onPollTimer);
        uploadTicker.attach_ms(deviceConfig.upload_interval_ms, onUploadTimer);

        // Start configuration request timer (every 5 minutes)
        configRequestTicker.attach_ms(300000, onConfigRequestTimer);

        Serial.println("[MAIN] System initialized successfully");
        printSystemStatus();
    }
    else
    {
        Serial.println("[MAIN] System initialization failed!");
    }
}

void loop()
{
    // Handle serial commands for configuration
    handleSerialCommands();

    // Basic watchdog - restart if system hangs
    static unsigned long lastLoopTime = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastLoopTime > 60000)
    { // 1 minute timeout
        Serial.println("[WATCHDOG] Loop timeout - restarting");
        ESP.restart();
    }
    lastLoopTime = currentTime;

    // Small delay to prevent watchdog reset
    // Check for deferred tasks set by timers
    if (pollPending)
    {
        // clear flag before executing to avoid missing new requests
        pollPending = false;
        pollSensors();
    }

    if (uploadPending)
    {
        uploadPending = false;
        uploadData();
    }

    if (configRequestPending)
    {
        configRequestPending = false;
        requestConfigUpdate();
    }

    // Execute pending commands
    if (pendingCommand.valid)
    {
        executeCommand();
    }

    delay(100);
}

bool initializeSystem()
{
    Serial.println("[INIT] Starting system initialization...");

    // Initialize configuration manager
    if (!configManager.begin())
    {
        Serial.println("[INIT] Failed to load configuration, using defaults");
    }

    // Configure inverter with slave address from configuration
    const DeviceConfig &deviceConfig = configManager.getDeviceConfig();
    inverter.setSlaveAddress(deviceConfig.slave_address);
    Serial.print("[INIT] Inverter slave address set to: 0x");
    Serial.println(deviceConfig.slave_address, HEX);

    // Initialize inverter communication
    if (!inverter.begin())
    {
        Serial.println("[INIT] Failed to initialize inverter communication");
        return false;
    }

    Serial.println("[INIT] System initialization complete");
    return true;
}

void setupPollingConfig()
{
    Serial.println("[CONFIG] Setting up polling configuration...");

    // Read parameters from configManager instead of hardcoded values
    const DeviceConfig &deviceConfig = configManager.getDeviceConfig();
    std::vector<ParameterType> params;

    for (uint8_t i = 0; i < deviceConfig.num_enabled_params; ++i)
    {
        params.push_back(deviceConfig.enabled_params[i]);
    }

    pollingConfig.setParameters(params);
    pollingConfig.printEnabledParameters();
}

void applyNewConfiguration()
{
    Serial.println("[CONFIG] Applying new configuration...");

    // Reconfigure polling parameters
    setupPollingConfig();

    // Update polling timer with new interval
    const DeviceConfig &deviceConfig = configManager.getDeviceConfig();
    pollTicker.detach();
    pollTicker.attach_ms(deviceConfig.poll_interval_ms, onPollTimer);

    Serial.print("[CONFIG] New polling interval: ");
    Serial.print(deviceConfig.poll_interval_ms);
    Serial.println(" ms");
}

void pollSensors()
{
    if (!systemInitialized)
        return;

    Serial.println("[POLL] Starting sensor polling...");

    Sample sample;
    sample.timestamp = millis() - startTime;

    bool allSuccess = true;
    const auto &enabledParams = pollingConfig.getEnabledParameters();

    for (ParameterType paramType : enabledParams)
    {
        float value;

        if (inverter.read(paramType, value))
        {
            sample.setValue(paramType, value);
            // Get name and unit from parameter descriptor table
            String friendlyName = pollingConfig.getParameterName(paramType);
            String unit = pollingConfig.getParameterUnit(paramType);

            // Fallback to default conversion if name/unit not found
            if (friendlyName.length() == 0)
            {
                friendlyName = parameterTypeToString(paramType);
            }
            if (unit.length() == 0)
            {
                switch (paramType)
                {
                case ParameterType::AC_VOLTAGE:
                case ParameterType::PV1_VOLTAGE:
                case ParameterType::PV2_VOLTAGE:
                    unit = " V";
                    break;
                case ParameterType::AC_CURRENT:
                case ParameterType::PV1_CURRENT:
                case ParameterType::PV2_CURRENT:
                    unit = " A";
                    break;
                case ParameterType::AC_FREQUENCY:
                    unit = " Hz";
                    break;
                case ParameterType::TEMPERATURE:
                    unit = " °C";
                    break;
                case ParameterType::OUTPUT_POWER:
                    unit = " W";
                    break;
                case ParameterType::EXPORT_POWER_PERCENT:
                    unit = " %";
                    break;
                default:
                    unit = "";
                    break;
                }
            }
            Serial.print("[POLL] ");
            Serial.print(friendlyName);
            Serial.print(": ");
            Serial.print(value, 2);
            Serial.println(unit);
        }
        else
        {
            Serial.print("[POLL] Failed to read ");
            String paramName = pollingConfig.getParameterName(paramType);
            if (paramName.length() > 0)
            {
                Serial.println(paramName);
            }
            else
            {
                Serial.println(parameterTypeToString(paramType));
            }
            allSuccess = false;
        }
    }

    if (allSuccess && dataBuffer.hasSpace())
    {
        dataBuffer.append(sample);
        Serial.print("[BUFFER] Sample added, buffer size: ");
        Serial.println(dataBuffer.size());
    }
    else if (!allSuccess)
    {
        Serial.println("[POLL] Poll failed for some parameters");
    }
    else
    {
        Serial.println("[BUFFER] Buffer full, sample discarded");
    }
}

void uploadData()
{
    if (!systemInitialized || dataBuffer.empty())
    {
        Serial.println("[UPLOAD] No data to upload");
        return;
    }

    static bool uploadInProgress = false;
    if (uploadInProgress)
    {
        Serial.println("[UPLOAD] Previous upload still in progress; skipping this tick");
        return;
    }
    uploadInProgress = true;

    Serial.println("[UPLOAD] Starting data upload...");

    // Non-destructive snapshot; clear only after ACK success
    auto samples = dataBuffer.snapshot();
    Serial.print("[UPLOAD] Uploading ");
    Serial.print(samples.size());
    Serial.println(" samples");

    if (uploadToServer(samples))
    {
        Serial.println("[UPLOAD] Upload successful");
        dataBuffer.clear();

        // Clear command result after successful upload
        if (lastCommandResult.has_result)
        {
            Serial.println("[COMMAND] Command result successfully reported to cloud");
            lastCommandResult.has_result = false;
        }

        // Apply pending configuration changes after successful upload
        if (pendingConfigurationUpdate)
        {
            Serial.println("[CONFIG] Applying pending configuration changes...");
            applyNewConfiguration();
            pendingConfigurationUpdate = false;
            Serial.println("[CONFIG] New configuration applied successfully");
        }
    }
    else
    {
        Serial.println("[UPLOAD] Upload failed");
    }

    uploadInProgress = false;
}

void requestConfigUpdate()
{
    if (!systemInitialized)
    {
        Serial.println("[CONFIG] System not initialized, skipping config request");
        return;
    }

    static bool configRequestInProgress = false;
    if (configRequestInProgress)
    {
        Serial.println("[CONFIG] Previous config request still in progress; skipping this tick");
        return;
    }
    configRequestInProgress = true;

    Serial.println("[CONFIG] Requesting configuration update from cloud...");

    if (sendConfigRequest())
    {
        Serial.println("[CONFIG] Configuration request successful");
    }
    else
    {
        Serial.println("[CONFIG] Configuration request failed");
    }

    configRequestInProgress = false;
}

bool sendConfigRequest()
{
    const APIConfig &apiConfig = configManager.getAPIConfig();
    HTTPClient httpClient;
    WiFiClient wifiClient;

    // Use config_url endpoint for configuration requests, fallback to upload_url
    const char *configUrl;
    if (strlen(apiConfig.config_url) > 0)
        configUrl = apiConfig.config_url;
    else if (strlen(apiConfig.upload_url) > 0)
        configUrl = apiConfig.upload_url;
    else
        configUrl = "http://10.63.73.102:5000/config";

    httpClient.begin(wifiClient, configUrl);
    Serial.print("[HTTP] Config request to: ");
    Serial.println(configUrl);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.setTimeout(apiConfig.timeout_ms);

    // Build device status request as per specification
    StaticJsonDocument<512> requestDoc;
    requestDoc["device_id"] = WiFi.hostname();
    requestDoc["status"] = "ready";

    String payload;
    serializeJson(requestDoc, payload);

    Serial.print("[HTTP] Config request payload: ");
    Serial.println(payload);

    const int maxAttempts = 2;
    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        int code = httpClient.POST(payload);
        if (code > 0)
        {
            String response = httpClient.getString();
            Serial.print("[HTTP] Config response code: ");
            Serial.println(code);
            Serial.print("[HTTP] Config response: ");
            Serial.println(response);

            if (code == HTTP_CODE_OK)
            {
                StaticJsonDocument<1024> respDoc;
                if (deserializeJson(respDoc, response) == DeserializationError::Ok)
                {
                    // Check for config_update message format
                    if (respDoc.containsKey("config_update"))
                    {
                        JsonObject configUpdate = respDoc["config_update"].as<JsonObject>();
                        Serial.print("[CONFIG] Received config_update: ");
                        serializeJson(configUpdate, Serial);
                        Serial.println();

                        // Parse configuration update
                        bool configValid = true;
                        std::vector<String> acceptedParams;
                        std::vector<String> rejectedParams;
                        std::vector<String> unchangedParams;

                        uint16_t newInterval = 0;
                        std::vector<ParameterType> newParams;

                        // Parse sampling_interval
                        if (configUpdate.containsKey("sampling_interval"))
                        {
                            newInterval = configUpdate["sampling_interval"] | 0;
                            if (newInterval > 0 && newInterval >= 1000 && newInterval <= 60000)
                            {
                                acceptedParams.push_back("sampling_interval");
                                Serial.print("[CONFIG] New sampling interval: ");
                                Serial.print(newInterval);
                                Serial.println(" ms");
                            }
                            else
                            {
                                rejectedParams.push_back("sampling_interval");
                                Serial.println("[CONFIG] Error: Invalid sampling_interval (must be 1000-60000ms)");
                                configValid = false;
                            }
                        }

                        // Parse registers array
                        if (configUpdate.containsKey("registers"))
                        {
                            JsonArray registersArray = configUpdate["registers"];
                            if (!registersArray.isNull())
                            {
                                bool registersValid = true;
                                for (JsonVariant reg : registersArray)
                                {
                                    String regStr = reg.as<String>();

                                    // Map cloud register names to our parameter types
                                    ParameterType paramType;
                                    if (regStr == "voltage")
                                        paramType = ParameterType::AC_VOLTAGE;
                                    else if (regStr == "current")
                                        paramType = ParameterType::AC_CURRENT;
                                    else if (regStr == "frequency")
                                        paramType = ParameterType::AC_FREQUENCY;
                                    else if (regStr == "temperature")
                                        paramType = ParameterType::TEMPERATURE;
                                    else if (regStr == "power")
                                        paramType = ParameterType::OUTPUT_POWER;
                                    else if (regStr == "pv1_voltage")
                                        paramType = ParameterType::PV1_VOLTAGE;
                                    else if (regStr == "pv2_voltage")
                                        paramType = ParameterType::PV2_VOLTAGE;
                                    else if (regStr == "pv1_current")
                                        paramType = ParameterType::PV1_CURRENT;
                                    else if (regStr == "pv2_current")
                                        paramType = ParameterType::PV2_CURRENT;
                                    else if (regStr == "export_power_percent")
                                        paramType = ParameterType::EXPORT_POWER_PERCENT;
                                    else
                                    {
                                        Serial.print("[CONFIG] Error: Invalid register '");
                                        Serial.print(regStr);
                                        Serial.println("' - skipping");
                                        registersValid = false;
                                        continue;
                                    }

                                    newParams.push_back(paramType);
                                    Serial.print("[CONFIG] Valid register: ");
                                    Serial.println(regStr);
                                }

                                if (registersValid && !newParams.empty())
                                {
                                    acceptedParams.push_back("registers");
                                }
                                else
                                {
                                    rejectedParams.push_back("registers");
                                    if (newParams.empty())
                                        Serial.println("[CONFIG] Error: No valid registers found");
                                    configValid = false;
                                }
                            }
                            else
                            {
                                rejectedParams.push_back("registers");
                                Serial.println("[CONFIG] Error: Invalid registers array");
                                configValid = false;
                            }
                        }

                        // Store configuration if valid (but don't apply immediately)
                        if (configValid && (!acceptedParams.empty()))
                        {
                            Serial.println("[CONFIG] Storing new configuration for next upload cycle...");

                            // Update configuration only for accepted parameters
                            if (newInterval > 0 && std::find(acceptedParams.begin(), acceptedParams.end(), "sampling_interval") != acceptedParams.end())
                            {
                                if (!newParams.empty() && std::find(acceptedParams.begin(), acceptedParams.end(), "registers") != acceptedParams.end())
                                {
                                    configManager.updatePollingConfig(newInterval, newParams);
                                }
                                else
                                {
                                    // Only update interval, keep current parameters
                                    const DeviceConfig &currentConfig = configManager.getDeviceConfig();
                                    std::vector<ParameterType> currentParams;
                                    for (uint8_t i = 0; i < currentConfig.num_enabled_params; ++i)
                                    {
                                        currentParams.push_back(currentConfig.enabled_params[i]);
                                    }
                                    configManager.updatePollingConfig(newInterval, currentParams);
                                }
                            }
                            else if (!newParams.empty() && std::find(acceptedParams.begin(), acceptedParams.end(), "registers") != acceptedParams.end())
                            {
                                // Only update parameters, keep current interval
                                const DeviceConfig &currentConfig = configManager.getDeviceConfig();
                                configManager.updatePollingConfig(currentConfig.poll_interval_ms, newParams);
                            }

                            if (configManager.saveConfig())
                            {
                                Serial.println("[CONFIG] Configuration saved to EEPROM");
                                // Mark that we have a pending configuration update to apply after next successful upload
                                pendingConfigurationUpdate = true;
                                Serial.println("[CONFIG] Configuration will take effect after next successful upload cycle");
                            }
                            else
                            {
                                Serial.println("[CONFIG] Error: Failed to save configuration");
                                rejectedParams.insert(rejectedParams.end(), acceptedParams.begin(), acceptedParams.end());
                                acceptedParams.clear();
                            }
                        }
                        else if (!acceptedParams.empty())
                        {
                            Serial.println("[CONFIG] Configuration update rejected due to validation errors");
                        }

                        // Send acknowledgment response
                        StaticJsonDocument<512> ackDoc;
                        JsonObject configAck = ackDoc.createNestedObject("config_ack");

                        JsonArray acceptedArray = configAck.createNestedArray("accepted");
                        for (const String &param : acceptedParams)
                        {
                            acceptedArray.add(param);
                        }

                        JsonArray rejectedArray = configAck.createNestedArray("rejected");
                        for (const String &param : rejectedParams)
                        {
                            rejectedArray.add(param);
                        }

                        JsonArray unchangedArray = configAck.createNestedArray("unchanged");
                        for (const String &param : unchangedParams)
                        {
                            unchangedArray.add(param);
                        }

                        String ackPayload;
                        serializeJson(ackDoc, ackPayload);
                        Serial.print("[CONFIG] Sending acknowledgment: ");
                        Serial.println(ackPayload);

                        httpClient.end();
                        return true;
                    }
                    // Check for command message format
                    else if (respDoc.containsKey("command"))
                    {
                        JsonObject command = respDoc["command"].as<JsonObject>();
                        Serial.print("[COMMAND] Received command: ");
                        serializeJson(command, Serial);
                        Serial.println();

                        // Parse command
                        String action = command["action"] | "";
                        String target_register = command["target_register"] | "";
                        int value = command["value"] | 0;

                        // Validate command
                        if (action == "write_register" && target_register.length() > 0)
                        {
                            // Store the command for execution
                            pendingCommand.action = action;
                            pendingCommand.target_register = target_register;
                            pendingCommand.value = value;
                            pendingCommand.received_at = millis();
                            pendingCommand.valid = true;

                            Serial.print("[COMMAND] Queued write command: register=");
                            Serial.print(target_register);
                            Serial.print(", value=");
                            Serial.println(value);
                        }
                        else
                        {
                            Serial.println("[COMMAND] Error: Invalid command format");
                        }

                        httpClient.end();
                        return true;
                    }
                    else
                    {
                        // No configuration update or command available
                        Serial.println("[CONFIG] No configuration update or command available");
                        httpClient.end();
                        return true;
                    }
                }
                else
                {
                    Serial.println("[CONFIG] Failed to parse JSON response");
                }
            }
            else
            {
                Serial.print("[CONFIG] HTTP error code: ");
                Serial.println(code);
            }
        }
        else
        {
            Serial.print("[CONFIG] HTTP error: ");
            Serial.println(httpClient.errorToString(code));
        }

        if (attempt < maxAttempts - 1)
        {
            Serial.println("[CONFIG] Retrying configuration request...");
            delay(2000);
        }
    }

    httpClient.end();
    return false;
}

void executeCommand()
{
    if (!pendingCommand.valid)
        return;

    Serial.println("[COMMAND] Executing pending command...");

    CommandResult result = {"", "", "", false};

    if (pendingCommand.action == "write_register")
    {
        if (executeWriteRegisterCommand(pendingCommand.target_register, pendingCommand.value, result))
        {
            result.status = "success";
            // Generate ISO8601 timestamp
            unsigned long currentTime = millis();
            result.executed_at = "2025-10-10T" + String((currentTime / 3600000) % 24, DEC) + ":" +
                                 String((currentTime / 60000) % 60, DEC) + ":" +
                                 String((currentTime / 1000) % 60, DEC) + "Z";
            Serial.println("[COMMAND] Command executed successfully");
        }
        else
        {
            result.status = "error";
            if (result.error_message.length() == 0)
                result.error_message = "Command execution failed";
            Serial.print("[COMMAND] Command execution failed: ");
            Serial.println(result.error_message);
        }
    }
    else
    {
        result.status = "error";
        result.error_message = "Unsupported action: " + pendingCommand.action;
        Serial.print("[COMMAND] Unsupported action: ");
        Serial.println(pendingCommand.action);
    }

    // Store result for next transmission
    lastCommandResult = result;
    lastCommandResult.has_result = true;

    // Clear pending command
    pendingCommand.valid = false;
}

bool executeWriteRegisterCommand(const String &register_name, int value, CommandResult &result)
{
    Serial.print("[COMMAND] Writing to register: ");
    Serial.print(register_name);
    Serial.print(" = ");
    Serial.println(value);

    // Map register names to our system
    // Only register 8 (export power percentage) is writable according to specification
    if (register_name == "export_power_percent")
    {
        // Use the inverter's setExportPowerPercent method
        if (inverter.setExportPowerPercent(value))
        {
            Serial.print("[COMMAND] Successfully wrote ");
            Serial.print(value);
            Serial.print(" to ");
            Serial.println(register_name);
            return true;
        }
        else
        {
            result.error_message = "Failed to write to inverter register";
            Serial.println("[COMMAND] Error: Failed to write to inverter register");
            return false;
        }
    }
    else
    {
        result.error_message = "Register '" + register_name + "' is not writable";
        Serial.print("[COMMAND] Error: Register '");
        Serial.print(register_name);
        Serial.println("' is not writable");
        return false;
    }
}

bool uploadToServer(const std::vector<Sample> &samples)
{
    const APIConfig &apiConfig = configManager.getAPIConfig();
    HTTPClient httpClient;
    WiFiClient wifiClient;

    // Use upload_url for cloud ingestion
    const char *uploadUrl = apiConfig.upload_url[0] != '\0' ? apiConfig.upload_url : "http://10.63.73.102:5000/upload";
    httpClient.begin(wifiClient, uploadUrl);
    Serial.print("[HTTP] POST to: ");
    Serial.println(uploadUrl);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.setTimeout(apiConfig.timeout_ms);

    // Build payload expected by Flask app.py with compression + aggregation
    DynamicJsonDocument jsonDoc(8192);
    // Session/window metadata
    static uint32_t session_counter = 0;
    // Derive window timing from first/last sample timestamps
    uint32_t window_start = samples.empty() ? 0 : samples.front().timestamp;
    uint32_t window_end = samples.empty() ? 0 : samples.back().timestamp;
    jsonDoc["device_id"] = WiFi.hostname();
    jsonDoc["timestamp"] = millis() - startTime; // client send time
    jsonDoc["session_id"] = (uint32_t)(ESP.getChipId() ^ millis() ^ (++session_counter));
    jsonDoc["window_start_ms"] = window_start;
    jsonDoc["window_end_ms"] = window_end;
    jsonDoc["poll_count"] = (int)samples.size();

    // Include command result if available
    if (lastCommandResult.has_result)
    {
        JsonObject commandResult = jsonDoc.createNestedObject("command_result");
        commandResult["status"] = lastCommandResult.status;
        if (lastCommandResult.executed_at.length() > 0)
            commandResult["executed_at"] = lastCommandResult.executed_at;
        if (lastCommandResult.error_message.length() > 0)
            commandResult["error_message"] = lastCommandResult.error_message;

        Serial.print("[COMMAND] Including command result in upload: ");
        serializeJson(commandResult, Serial);
        Serial.println();
    }

    // Accumulators for a single-document (non-chunked) build
    size_t totalOriginalBytes = 0;   // sum of 4 * n_samples per field
    size_t totalCompressedBytes = 0; // sum of varint-encoded bytes_len per field
    float totalCpuMs = 0.0f;         // sum of cpu_time_ms per field
    bool verifyAll = true;           // AND of per-field verify_ok

    // Helper to append a single parameter field into a doc and accumulate totals
    auto appendParamField = [&](DynamicJsonDocument &doc, ParameterType param)
    {
        JsonObject fieldsObjLoc = doc["fields"].isNull() ? doc.createNestedObject("fields") : doc["fields"].as<JsonObject>();

        // Collect scaled integer series for this parameter
        std::vector<long> series;
        series.reserve(samples.size());
        for (const auto &s : samples)
        {
            if (!s.hasValue(param))
                continue;
            float v = s.getValue(param);
            long scaled = 0;
            if (param == ParameterType::AC_VOLTAGE || param == ParameterType::AC_CURRENT || param == ParameterType::AC_FREQUENCY)
                scaled = (long)roundf(v * 1000.0f);
            else
                scaled = (long)roundf(v);
            series.push_back(scaled);
        }
        if (series.empty())
            return; // nothing to add

        long minV = series[0], maxV = series[0];
        long sum = 0;
        for (long x : series)
        {
            if (x < minV)
                minV = x;
            if (x > maxV)
                maxV = x;
            sum += x;
        }
        float avg = (float)sum / (float)series.size();

        unsigned long t0 = micros();
        std::vector<int32_t> deltas;
        Compression::delta_compress(series, deltas);
        std::vector<uint8_t> varintBytes;
        Compression::encode_deltas_varint(deltas, varintBytes);
        bool verify_ok = true;
        {
            std::vector<int32_t> deltas2;
            if (!Compression::decode_deltas_varint(varintBytes, deltas2))
                verify_ok = false;
            else
            {
                std::vector<long> recon;
                Compression::delta_decompress(deltas2, recon);
                if (recon.size() != series.size())
                    verify_ok = false;
                else
                {
                    for (size_t i = 0; i < recon.size(); ++i)
                    {
                        if (recon[i] != series[i])
                        {
                            verify_ok = false;
                            break;
                        }
                    }
                }
            }
        }
        unsigned long t1 = micros();
        float cpu_ms = (t1 - t0) / 1000.0f;
        size_t bytes_len = varintBytes.size();

        String paramName = parameterTypeToString(param);
        JsonObject field = fieldsObjLoc.createNestedObject(paramName.c_str());
        field["method"] = "Delta";
        field["param_id"] = static_cast<int>(param);
        field["n_samples"] = (int)series.size();
        field["bytes_len"] = (int)bytes_len;
        field["cpu_time_ms"] = cpu_ms;
        field["verify_ok"] = verify_ok;
        field["original_bytes"] = (int)(4 * series.size());

        JsonObject agg = field.createNestedObject("agg");
        agg["min"] = (long)minV;
        agg["avg"] = avg;
        agg["max"] = (long)maxV;

        JsonArray payloadArr = field.createNestedArray("payload");
        for (auto d : deltas)
            payloadArr.add((long)d);
        field["payload_varint_hex"] = Compression::hex_encode(varintBytes);

        // Accumulate upload-level totals
        totalOriginalBytes += (4 * series.size());
        totalCompressedBytes += bytes_len;
        totalCpuMs += cpu_ms;
        verifyAll = verifyAll && verify_ok;
    };

    const auto enabledParams = pollingConfig.getEnabledParameters();

    // First, attempt to build a single-chunk payload and see if it fits
    for (ParameterType p : enabledParams)
    {
        appendParamField(jsonDoc, p);
    }

    // Add upload-level metadata for original/compressed sizes and verification
    jsonDoc["original_payload_size_bytes_total"] = (int)totalOriginalBytes;
    jsonDoc["compressed_payload_size_bytes_total"] = (int)totalCompressedBytes;
    jsonDoc["cpu_time_ms_total"] = totalCpuMs;
    jsonDoc["verify_ok_all"] = verifyAll;

    const size_t PAYLOAD_THRESHOLD = 3500; // bytes; adjust if needed

    // Function to send a document with retry/backoff
    auto sendWithRetry = [&](DynamicJsonDocument &doc) -> bool
    {
        // Compute MAC over JSON without mac field
        String tmp;
        serializeJson(doc, tmp);
        uint32_t macLocal = crc32_calc(reinterpret_cast<const uint8_t *>(tmp.c_str()), tmp.length());
        doc["mac_crc32"] = macLocal;
        tmp = String();
        serializeJson(doc, tmp);

        Serial.print("[HTTP] Payload size: ");
        Serial.println(tmp.length());

        const int maxAttempts = 3;
        int attemptLocal = 0;
        while (attemptLocal < maxAttempts)
        {
            int code = httpClient.POST(tmp);
            if (code > 0)
            {
                String response = httpClient.getString();
                Serial.print("[HTTP] Response code: ");
                Serial.println(code);
                Serial.print("[HTTP] Response: ");
                Serial.println(response);

                if (code == HTTP_CODE_OK)
                {
                    StaticJsonDocument<1024> respDoc;
                    if (deserializeJson(respDoc, response) == DeserializationError::Ok)
                    {
                        const char *status = respDoc["status"] | "";
                        if (strcmp(status, "ok") == 0)
                        {
                            return true;
                        }
                    }
                }
            }
            else
            {
                Serial.print("[HTTP] Error: ");
                Serial.println(httpClient.errorToString(code));
            }
            ++attemptLocal;
            if (attemptLocal < maxAttempts)
            {
                uint32_t backoffMs = (1u << (attemptLocal - 1)) * 1000u;
                if (backoffMs > 4000u)
                    backoffMs = 4000u;
                Serial.print("[HTTP] Retry attempt ");
                Serial.print(attemptLocal + 1);
                Serial.print(" in ");
                Serial.print(backoffMs);
                Serial.println(" ms");
                delay(backoffMs);
            }
        }
        return false;
    };

    // If the single doc is too large, chunk by fields
    String firstPayload;
    serializeJson(jsonDoc, firstPayload);
    if (firstPayload.length() <= PAYLOAD_THRESHOLD)
    {
        bool ok = sendWithRetry(jsonDoc);
        httpClient.end();
        return ok;
    }

    Serial.println("[UPLOAD] Payload exceeds threshold, chunking by fields");

    // Build per-field chunks
    struct FieldNameParam
    {
        String name;
        ParameterType param;
    };
    std::vector<FieldNameParam> fieldList;
    for (auto p : enabledParams)
    {
        FieldNameParam f{parameterTypeToString(p), p};
        fieldList.push_back(f);
    }

    // Helper to append a single parameter into a chunk doc and accumulate chunk-level totals only
    auto appendParamFieldChunk = [&](DynamicJsonDocument &doc, ParameterType param,
                                     size_t &chunkOriginalBytes, size_t &chunkCompressedBytes,
                                     float &chunkCpuMs, bool &chunkVerifyAll)
    {
        JsonObject fieldsObjLoc = doc["fields"].isNull() ? doc.createNestedObject("fields") : doc["fields"].as<JsonObject>();

        // Collect scaled integer series for this parameter
        std::vector<long> series;
        series.reserve(samples.size());
        for (const auto &s : samples)
        {
            if (!s.hasValue(param))
                continue;
            float v = s.getValue(param);
            long scaled = 0;
            if (param == ParameterType::AC_VOLTAGE || param == ParameterType::AC_CURRENT || param == ParameterType::AC_FREQUENCY)
                scaled = (long)roundf(v * 1000.0f);
            else
                scaled = (long)roundf(v);
            series.push_back(scaled);
        }
        if (series.empty())
            return; // nothing to add

        long minV = series[0], maxV = series[0];
        long sum = 0;
        for (long x : series)
        {
            if (x < minV)
                minV = x;
            if (x > maxV)
                maxV = x;
            sum += x;
        }
        float avg = (float)sum / (float)series.size();

        unsigned long t0 = micros();
        std::vector<int32_t> deltas;
        Compression::delta_compress(series, deltas);
        std::vector<uint8_t> varintBytes;
        Compression::encode_deltas_varint(deltas, varintBytes);
        bool verify_ok = true;
        {
            std::vector<int32_t> deltas2;
            if (!Compression::decode_deltas_varint(varintBytes, deltas2))
                verify_ok = false;
            else
            {
                std::vector<long> recon;
                Compression::delta_decompress(deltas2, recon);
                if (recon.size() != series.size())
                    verify_ok = false;
                else
                {
                    for (size_t i = 0; i < recon.size(); ++i)
                    {
                        if (recon[i] != series[i])
                        {
                            verify_ok = false;
                            break;
                        }
                    }
                }
            }
        }
        unsigned long t1 = micros();
        float cpu_ms = (t1 - t0) / 1000.0f;
        size_t bytes_len = varintBytes.size();

        String paramName = parameterTypeToString(param);
        JsonObject field = fieldsObjLoc.createNestedObject(paramName.c_str());
        field["method"] = "Delta";
        field["param_id"] = static_cast<int>(param);
        field["n_samples"] = (int)series.size();
        field["bytes_len"] = (int)bytes_len;
        field["cpu_time_ms"] = cpu_ms;
        field["verify_ok"] = verify_ok;
        field["original_bytes"] = (int)(4 * series.size());

        JsonObject agg = field.createNestedObject("agg");
        agg["min"] = (long)minV;
        agg["avg"] = avg;
        agg["max"] = (long)maxV;

        JsonArray payloadArr = field.createNestedArray("payload");
        for (auto d : deltas)
            payloadArr.add((long)d);
        field["payload_varint_hex"] = Compression::hex_encode(varintBytes);

        // Accumulate chunk-level totals only
        chunkOriginalBytes += (4 * series.size());
        chunkCompressedBytes += bytes_len;
        chunkCpuMs += cpu_ms;
        chunkVerifyAll = chunkVerifyAll && verify_ok;
    };

    // Try sequentially packing fields until threshold is reached, send chunk, then continue
    size_t total = 0;
    {
        // compute total chunks by simulating packing
        size_t countChunks = 0;
        size_t idx = 0;
        while (idx < fieldList.size())
        {
            DynamicJsonDocument docChunk(8192);
            // copy session/window metadata
            for (JsonPair kv : jsonDoc.as<JsonObject>())
            {
                if (String(kv.key().c_str()) == "fields")
                    continue;
                docChunk[kv.key().c_str()] = kv.value();
            }
            JsonObject fieldsObjC = docChunk.createNestedObject("fields");
            size_t startIdx = idx;
            for (; idx < fieldList.size(); ++idx)
            {
                appendParamField(docChunk, fieldList[idx].param);
                String probe;
                serializeJson(docChunk, probe);
                if (probe.length() > PAYLOAD_THRESHOLD)
                {
                    // remove the last addition by rebuilding chunk without it
                    docChunk.remove("fields");
                    fieldsObjC = docChunk.createNestedObject("fields");
                    for (size_t j = startIdx; j < idx; ++j)
                        appendParamField(docChunk, fieldList[j].param);
                    break;
                }
            }
            countChunks++;
        }
        total = countChunks;
    }

    size_t seq = 1;
    size_t idx = 0;
    while (idx < fieldList.size())
    {
        DynamicJsonDocument docChunk(8192);
        for (JsonPair kv : jsonDoc.as<JsonObject>())
        {
            if (String(kv.key().c_str()) == "fields")
                continue;
            docChunk[kv.key().c_str()] = kv.value();
        }
        docChunk["chunk_seq"] = (int)seq;
        docChunk["chunk_total"] = (int)total;
        JsonObject fieldsObjC = docChunk.createNestedObject("fields");

        // Chunk accumulators
        size_t chunkOriginalBytes = 0;
        size_t chunkCompressedBytes = 0;
        float chunkCpuMs = 0.0f;
        bool chunkVerifyAll = true;

        size_t startIdx = idx;
        for (; idx < fieldList.size(); ++idx)
        {
            appendParamFieldChunk(docChunk, fieldList[idx].param, chunkOriginalBytes, chunkCompressedBytes, chunkCpuMs, chunkVerifyAll);
            String probe;
            serializeJson(docChunk, probe);
            if (probe.length() > PAYLOAD_THRESHOLD)
            {
                // rebuild without last
                docChunk.remove("fields");
                fieldsObjC = docChunk.createNestedObject("fields");
                for (size_t j = startIdx; j < idx; ++j)
                    appendParamFieldChunk(docChunk, fieldList[j].param, chunkOriginalBytes, chunkCompressedBytes, chunkCpuMs, chunkVerifyAll);
                break;
            }
        }

        // Add window totals and chunk subtotals
        docChunk["original_payload_size_bytes_total"] = (int)totalOriginalBytes;
        docChunk["compressed_payload_size_bytes_total"] = (int)totalCompressedBytes;
        docChunk["cpu_time_ms_total_window"] = totalCpuMs;
        docChunk["verify_ok_all_window"] = verifyAll;

        docChunk["original_payload_size_bytes_chunk"] = (int)chunkOriginalBytes;
        docChunk["compressed_payload_size_bytes_chunk"] = (int)chunkCompressedBytes;
        docChunk["cpu_time_ms_chunk"] = chunkCpuMs;
        docChunk["verify_ok_all_chunk"] = chunkVerifyAll;

        bool ok = sendWithRetry(docChunk);
        if (!ok)
        {
            httpClient.end();
            return false;
        }
        ++seq;
    }

    httpClient.end();
    return true;
}

void printSystemStatus()
{
    Serial.println("\n==== SYSTEM STATUS ====");

    // WiFi status
    Serial.print("WiFi Status: ");
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(WiFi.SSID());
        Serial.print(" (");
        Serial.print(WiFi.localIP());
        Serial.println(")");
    }
    else
    {
        Serial.println("Not Connected");
    }

    // Memory status
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");

    // Buffer status
    Serial.print("Buffer Size: ");
    Serial.print(dataBuffer.size());
    Serial.print("/");
    Serial.println(configManager.getDeviceConfig().buffer_size);

    // Configuration
    Serial.print("Poll Interval: ");
    Serial.print(configManager.getDeviceConfig().poll_interval_ms);
    Serial.println(" ms");

    Serial.print("Upload Interval: ");
    Serial.print(configManager.getDeviceConfig().upload_interval_ms);
    Serial.println(" ms");

    Serial.println("Config Request Interval: 300000 ms (5 minutes)");

    // Pending configuration status
    if (pendingConfigurationUpdate)
    {
        Serial.println("Pending Config Update: YES (will apply after next upload)");
    }
    else
    {
        Serial.println("Pending Config Update: NO");
    }

    // Command execution status
    if (pendingCommand.valid)
    {
        Serial.print("Pending Command: ");
        Serial.print(pendingCommand.action);
        Serial.print(" ");
        Serial.print(pendingCommand.target_register);
        Serial.print(" = ");
        Serial.println(pendingCommand.value);
    }
    else
    {
        Serial.println("Pending Command: NO");
    }

    if (lastCommandResult.has_result)
    {
        Serial.print("Last Command Result: ");
        Serial.print(lastCommandResult.status);
        if (lastCommandResult.error_message.length() > 0)
        {
            Serial.print(" (");
            Serial.print(lastCommandResult.error_message);
            Serial.print(")");
        }
        Serial.println(" (will be reported on next upload)");
    }
    else
    {
        Serial.println("Last Command Result: NO");
    }

    Serial.println("========================\n");
}

void handleSerialCommands()
{
    if (Serial.available())
    {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "status")
        {
            printSystemStatus();
        }
        else if (command == "restart")
        {
            Serial.println("[CMD] Restarting system...");
            ESP.restart();
        }
        else if (command == "test")
        {
            Serial.println("[CMD] Running test poll...");
            pollSensors();
        }
        else if (command == "upload")
        {
            Serial.println("[CMD] Triggering upload...");
            uploadData();
        }
        else if (command == "config")
        {
            Serial.println("[CMD] Requesting configuration update...");
            requestConfigUpdate();
        }
        else if (command.startsWith("write "))
        {
            // Parse command: "write <register> <value>"
            int firstSpace = command.indexOf(' ');
            int secondSpace = command.indexOf(' ', firstSpace + 1);

            if (firstSpace > 0 && secondSpace > firstSpace)
            {
                String registerName = command.substring(firstSpace + 1, secondSpace);
                int value = command.substring(secondSpace + 1).toInt();

                Serial.print("[CMD] Testing write command: ");
                Serial.print(registerName);
                Serial.print(" = ");
                Serial.println(value);

                // Simulate a queued command
                pendingCommand.action = "write_register";
                pendingCommand.target_register = registerName;
                pendingCommand.value = value;
                pendingCommand.received_at = millis();
                pendingCommand.valid = true;

                Serial.println("[CMD] Command queued for execution");
            }
            else
            {
                Serial.println("[CMD] Usage: write <register> <value>");
                Serial.println("[CMD] Example: write export_power_percent 50");
            }
        }
        else if (command == "wifi")
        {
            Serial.print("[CMD] WiFi Status: ");
            Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
            if (WiFi.status() == WL_CONNECTED)
            {
                Serial.print("IP: ");
                Serial.println(WiFi.localIP());
                Serial.print("RSSI: ");
                Serial.println(WiFi.RSSI());
            }
        }
        else if (command == "help")
        {
            Serial.println("[CMD] Available commands:");
            Serial.println("  status  - Show system status");
            Serial.println("  restart - Restart the system");
            Serial.println("  test    - Run test sensor poll");
            Serial.println("  upload  - Trigger data upload");
            Serial.println("  config  - Request configuration update");
            Serial.println("  write <register> <value> - Test write command");
            Serial.println("  wifi    - Show WiFi status");
            Serial.println("  help    - Show this help");
        }
        else if (command.length() > 0)
        {
            Serial.println("[CMD] Unknown command. Type 'help' for available commands.");
        }
    }
}