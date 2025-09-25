#pragma once
#include <vector>
#include <map>
#include <cstdint>
#include <string>
#include "PollingConfig.h"

// RLE pair
struct RLEPair {
    int32_t val;
    int32_t count;
};

// Compression result per parameter
struct CompressionResult {
    ParameterType param;
    std::string method;
    size_t nSamples;
    size_t originalSize;
    size_t compressedSize;
    double ratio;
    double cpuTimeMs;
    bool verified;
    std::vector<int32_t> compressed_value; // decoded values for verification
};

// Mapping for ParameterType to string
inline std::string to_string(ParameterType param) {
    switch (param) {
        case ParameterType::AC_VOLTAGE:           return "AC_VOLTAGE";
        case ParameterType::AC_CURRENT:           return "AC_CURRENT";
        case ParameterType::AC_FREQUENCY:         return "AC_FREQUENCY";
        case ParameterType::PV1_VOLTAGE:          return "PV1_VOLTAGE";
        case ParameterType::PV2_VOLTAGE:          return "PV2_VOLTAGE";
        case ParameterType::PV1_CURRENT:          return "PV1_CURRENT";
        case ParameterType::PV2_CURRENT:          return "PV2_CURRENT";
        case ParameterType::TEMPERATURE:          return "TEMPERATURE";
        case ParameterType::EXPORT_POWER_PERCENT: return "EXPORT_POWER_PERCENT";
        case ParameterType::OUTPUT_POWER:         return "OUTPUT_POWER";
        default: return "UNKNOWN";
    }
}

// Core functions
std::vector<int32_t> deltaEncode(const std::vector<int32_t>& data);
std::vector<int32_t> deltaDecode(const std::vector<int32_t>& deltas);

std::vector<RLEPair> rleEncode(const std::vector<int32_t>& data);
std::vector<int32_t> rleDecode(const std::vector<RLEPair>& pairs);

// High-level: compress everything in buffer
std::vector<CompressionResult> compressBufferDelta(const std::vector<Sample>& samples, int scale = 1000);
std::vector<CompressionResult> compressBufferRle(const std::vector<Sample>& samples, int scale = 1000);
