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
};

// Core functions
std::vector<int32_t> deltaEncode(const std::vector<int32_t>& data);
std::vector<int32_t> deltaDecode(const std::vector<int32_t>& deltas);

std::vector<RLEPair> rleEncode(const std::vector<int32_t>& data);
std::vector<int32_t> rleDecode(const std::vector<RLEPair>& pairs);

// High-level: compress everything in buffer
std::vector<CompressionResult> compressBufferDelta(const std::vector<Sample>& samples, int scale = 1000);
std::vector<CompressionResult> compressBufferRle(const std::vector<Sample>& samples, int scale = 1000);
