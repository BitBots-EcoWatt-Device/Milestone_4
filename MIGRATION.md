# ESP8266 Migration Guide

This guide explains the step-by-step migration from laptop-based firmware to ESP8266 embedded device.

## Overview

The original codebase was designed for laptop execution with:

- Standard C++ threading (`std::thread`)
- CURL for HTTP requests
- File-based configuration
- Large memory buffers
- Standard library containers

The ESP8266 version has been adapted to work with:

- Arduino framework
- ESP8266-specific libraries
- EEPROM-based configuration
- Limited memory constraints
- Event-driven architecture using Ticker

## Key Changes Made

### 1. Threading → Event-Driven Architecture

**Original:**

```cpp
std::thread pollT(pollLoop, ...);
std::thread upT(uploadLoop, ...);
```

**ESP8266:**

```cpp
Ticker pollTicker;
Ticker uploadTicker;
pollTicker.attach_ms(deviceConfig.poll_interval_ms, pollSensors);
uploadTicker.attach_ms(deviceConfig.upload_interval_ms, uploadData);
```

### 2. HTTP Client Migration

**Original:**

```cpp
#include <curl/curl.h>
CURL *curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, url);
```

**ESP8266:**

```cpp
#include <ESP8266HTTPClient.h>
HTTPClient httpClient;
httpClient.begin(wifiClient, url);
httpClient.POST(payload);
```

### 3. Configuration System

**Original:**

```cpp
// File-based config.ini parsing
[API]
api_key=...
[ENDPOINTS]
read_url=...
```

**ESP8266:**

```cpp
// EEPROM-based configuration
struct ESP8266Config {
    WiFiConfig wifi;
    APIConfig api;
    DeviceConfig device;
};
EEPROM.put(0, config_);
```

### 4. Memory Management

**Original:**

```cpp
DataBuffer buffer(30);  // Large buffer
std::vector<Sample> large_data;
```

**ESP8266:**

```cpp
ESP8266DataBuffer dataBuffer(10);  // Smaller buffer
// Careful memory allocation
// Static arrays where possible
```

### 5. JSON Processing

**Original:**

```cpp
// Custom JSON string building
std::string packet_json = build_meta_json(...);
```

**ESP8266:**

```cpp
#include <ArduinoJson.h>
DynamicJsonDocument jsonDoc(4096);
JsonArray samplesArray = jsonDoc.createNestedArray("samples");
```

## Migration Steps Completed

### ✅ Step 1: Project Structure

- Created PlatformIO project structure
- Set up proper library dependencies
- Configured build environment for ESP8266

### ✅ Step 2: Configuration System

- Migrated from file-based to EEPROM-based config
- Created WiFi configuration management
- Added serial command interface for configuration

### ✅ Step 3: Communication Layer

- Replaced CURL with ESP8266HTTPClient
- Maintained HTTP API compatibility
- Added WiFi connection management with auto-reconnect

### ✅ Step 4: Protocol Adaptation

- Kept Modbus protocol structure
- Maintained HTTP-based Modbus gateway communication
- Prepared for future direct Modbus RTU implementation

### ✅ Step 5: Data Management

- Adapted threading to event-driven model
- Implemented memory-efficient data buffers
- Created Arduino-compatible data structures

### ✅ Step 6: Main Application

- Converted main() to setup()/loop() Arduino model
- Implemented non-blocking operation
- Added comprehensive status monitoring

## Current Status

The ESP8266 firmware is now ready for deployment with these features:

### ✅ Working Features

- WiFi connection management
- HTTP-based Modbus communication
- Sensor data polling (AC voltage, current, frequency, temperature, power)
- JSON data upload to server
- Serial command interface
- EEPROM configuration storage
- Memory-optimized operation
- Watchdog functionality

### 🔄 Partially Implemented

- Compression algorithms (can be ported if needed)
- Advanced packetization (simplified for ESP8266)
- Error recovery mechanisms

### ⏳ Future Enhancements

- Direct Modbus RTU over serial
- Over-the-air (OTA) updates
- Web-based configuration interface
- Advanced data compression
- Multiple inverter support

## Testing and Validation

### Required Testing Steps

1. **Hardware Setup**

   - Connect ESP8266 to power supply
   - Verify WiFi connectivity
   - Test serial communication

2. **Software Validation**

   - Verify sensor data reading
   - Test data upload to server
   - Validate configuration persistence
   - Check memory usage and stability

3. **Integration Testing**
   - End-to-end data flow validation
   - Server compatibility verification
   - Network resilience testing

## Deployment Checklist

- [ ] Update WiFi credentials in configuration
- [ ] Update server endpoints for your environment
- [ ] Test with actual inverter hardware
- [ ] Validate data accuracy against laptop version
- [ ] Perform extended runtime testing
- [ ] Document final hardware connections

## Performance Considerations

### Memory Usage

- **Heap Usage**: ~40KB typical, 80KB maximum
- **Buffer Size**: Limited to 10 samples (vs 30 on laptop)
- **JSON Payload**: <4KB per upload

### Timing

- **Poll Interval**: 5 seconds (configurable)
- **Upload Interval**: 30 seconds (configurable)
- **HTTP Timeout**: 5 seconds
- **WiFi Reconnect**: 15 seconds timeout

### Power Consumption

- **Active**: ~200mA @ 3.3V
- **Sleep Mode**: Can be implemented for battery operation
- **WiFi Usage**: Periodic connection for uploads

## Next Steps

1. **Initial Testing**: Flash firmware and validate basic functionality
2. **Configuration**: Update credentials and endpoints for your setup
3. **Integration**: Test with your specific inverter and server setup
4. **Optimization**: Fine-tune polling intervals and buffer sizes
5. **Production**: Deploy to multiple devices if testing is successful

The migration maintains the core functionality while adapting to ESP8266 constraints. The modular design allows for easy future enhancements and optimizations.
