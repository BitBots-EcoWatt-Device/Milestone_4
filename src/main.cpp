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

// Timers
Ticker pollTicker;
Ticker uploadTicker;

// Status variables
unsigned long startTime;
bool systemInitialized = false;

// Function prototypes
void setup();
void loop();
bool initializeSystem();
void pollSensors();
void uploadData();
void setupPollingConfig();
void printSystemStatus();
void handleSerialCommands();
bool uploadToServer(const std::vector<Sample> &samples);

// Flags set from timer callbacks (ISR context) to defer work to loop()
volatile bool pollPending = false;
volatile bool uploadPending = false;

void IRAM_ATTR onPollTimer()
{
    pollPending = true;
}

void IRAM_ATTR onUploadTimer()
{
    uploadPending = true;
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

    // Configure to poll key parameters
    std::vector<ParameterType> params = {
        ParameterType::AC_VOLTAGE,
        ParameterType::AC_CURRENT,
        ParameterType::AC_FREQUENCY,
        ParameterType::TEMPERATURE,
        ParameterType::OUTPUT_POWER};

    pollingConfig.setParameters(params);
    pollingConfig.printEnabledParameters();
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
        const ParameterConfig &paramConfig = pollingConfig.getParameterConfig(paramType);
        float value;

        if (paramConfig.readFunction(inverter, value))
        {
            sample.setValue(paramType, value);
            Serial.print("[POLL] ");
            Serial.print(paramConfig.name);
            Serial.print(": ");
            Serial.print(value);
            Serial.println(paramConfig.unit);
        }
        else
        {
            Serial.print("[POLL] Failed to read ");
            Serial.println(paramConfig.name);
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

    Serial.println("[UPLOAD] Starting data upload...");

    auto samples = dataBuffer.flush();
    Serial.print("[UPLOAD] Uploading ");
    Serial.print(samples.size());
    Serial.println(" samples");

    if (uploadToServer(samples))
    {
        Serial.println("[UPLOAD] Upload successful");
    }
    else
    {
        Serial.println("[UPLOAD] Upload failed");
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
    Serial.print("[HTTP] POST to: "); Serial.println(uploadUrl);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.setTimeout(apiConfig.timeout_ms);

    // Build payload expected by Flask app.py with compression + aggregation
    DynamicJsonDocument jsonDoc(8192);
    jsonDoc["device_id"] = WiFi.hostname();
    jsonDoc["timestamp"] = millis() - startTime;

    JsonObject fieldsObj = jsonDoc.createNestedObject("fields");

    // Build time-series per parameter across buffered samples
    const auto enabledParams = pollingConfig.getEnabledParameters();
    for (ParameterType param : enabledParams)
    {
        // Collect scaled integer series for this parameter
        std::vector<long> series;
        series.reserve(samples.size());
        for (const auto &s : samples)
        {
            if (!s.hasValue(param)) continue;
            float v = s.getValue(param);
            long scaled = 0;
            if (param == ParameterType::AC_VOLTAGE || param == ParameterType::AC_CURRENT || param == ParameterType::AC_FREQUENCY)
                scaled = (long)roundf(v * 1000.0f);
            else
                scaled = (long)roundf(v);
            series.push_back(scaled);
        }

        if (series.empty())
            continue;

        // Aggregations on scaled ints
        long minV = series[0], maxV = series[0];
        long sum = 0;
        for (long x : series) { if (x < minV) minV = x; if (x > maxV) maxV = x; sum += x; }
        float avg = (float)sum / (float)series.size();

        // Compress using delta encoding (send integer deltas for Flask compatibility)
        unsigned long t0 = micros();
        std::vector<int32_t> deltas;
        Compression::delta_compress(series, deltas);
        // Also compute varint+zigzag encoded bytes (real compact form) for reporting
        std::vector<uint8_t> varintBytes;
        Compression::encode_deltas_varint(deltas, varintBytes);
        // Optional self-verification: decode varints -> deltas -> samples and compare
        bool verify_ok = true;
        {
            std::vector<int32_t> deltas2;
            if (!Compression::decode_deltas_varint(varintBytes, deltas2))
            {
                verify_ok = false;
            }
            else
            {
                std::vector<long> recon;
                Compression::delta_decompress(deltas2, recon);
                if (recon.size() != series.size()) verify_ok = false;
                else
                {
                    for (size_t i = 0; i < recon.size(); ++i)
                    {
                        if (recon[i] != series[i]) { verify_ok = false; break; }
                    }
                }
            }
        }
        unsigned long t1 = micros();
        float cpu_ms = (t1 - t0) / 1000.0f;
        // Report the true compressed size using varint representation
        size_t bytes_len = varintBytes.size();

        String paramName = parameterTypeToString(param);
        JsonObject field = fieldsObj.createNestedObject(paramName.c_str());
        field["method"] = "Delta";
        field["param_id"] = static_cast<int>(param);
        field["n_samples"] = (int)series.size();
    field["bytes_len"] = (int)bytes_len;
        field["cpu_time_ms"] = cpu_ms;
    field["verify_ok"] = verify_ok;

        // Aggregates
        JsonObject agg = field.createNestedObject("agg");
        agg["min"] = (long)minV;
        agg["avg"] = avg; // keep as float for readability
        agg["max"] = (long)maxV;

        // Compressed payload (integer list of deltas for Flask display/decode)
        JsonArray payloadArr = field.createNestedArray("payload");
        for (auto d : deltas) payloadArr.add((long)d);

        // Also include varint hex for actual compact transport (server may ignore)
        field["payload_varint_hex"] = Compression::hex_encode(varintBytes);
    }

    String payload;
    serializeJson(jsonDoc, payload);

    Serial.print("[HTTP] Payload size: ");
    Serial.println(payload.length());

    int httpResponseCode = httpClient.POST(payload);

    if (httpResponseCode > 0)
    {
        String response = httpClient.getString();
        Serial.print("[HTTP] Response code: ");
        Serial.println(httpResponseCode);
        Serial.print("[HTTP] Response: ");
        Serial.println(response);

        httpClient.end();
        return httpResponseCode == HTTP_CODE_OK;
    }
    else
    {
        Serial.print("[HTTP] Error: ");
        Serial.println(httpClient.errorToString(httpResponseCode));
        httpClient.end();
        return false;
    }
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
            Serial.println("  wifi    - Show WiFi status");
            Serial.println("  help    - Show this help");
        }
        else if (command.length() > 0)
        {
            Serial.println("[CMD] Unknown command. Type 'help' for available commands.");
        }
    }
}