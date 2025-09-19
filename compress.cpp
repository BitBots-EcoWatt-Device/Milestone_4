#include "compress.h"
#include <chrono>
#include <cmath>

// Scale float to int for compression
inline int32_t scaleFloat(float v, int scale) {
    return static_cast<int32_t>(std::round(v * scale));
}

// ---------------- Delta ----------------
std::vector<int32_t> deltaEncode(const std::vector<int32_t>& data) {
    if (data.empty()) return {};
    std::vector<int32_t> out;
    out.reserve(data.size());
    out.push_back(data[0]);
    for (size_t i = 1; i < data.size(); i++) {
        out.push_back(data[i] - data[i - 1]);
    }
    return out;
}

std::vector<int32_t> deltaDecode(const std::vector<int32_t>& deltas) {
    if (deltas.empty()) return {};
    std::vector<int32_t> out;
    out.reserve(deltas.size());
    out.push_back(deltas[0]);
    for (size_t i = 1; i < deltas.size(); i++) {
        out.push_back(out.back() + deltas[i]);
    }
    return out;
}

// ---------------- RLE ----------------
std::vector<RLEPair> rleEncode(const std::vector<int32_t>& data) {
    std::vector<RLEPair> out;
    if (data.empty()) return out;
    int32_t prev = data[0];
    int32_t cnt = 1;
    for (size_t i = 1; i < data.size(); i++) {
        if (data[i] == prev) cnt++;
        else {
            out.push_back({prev, cnt});
            prev = data[i];
            cnt = 1;
        }
    }
    out.push_back({prev, cnt});
    return out;
}

std::vector<int32_t> rleDecode(const std::vector<RLEPair>& pairs) {
    std::vector<int32_t> out;
    for (auto& p : pairs) {
        for (int i = 0; i < p.count; i++) {
            out.push_back(p.val);
        }
    }
    return out;
}

// ---------------- High-level compression ----------------
std::vector<CompressionResult> compressBufferDelta(const std::vector<Sample>& samples, int scale) {
    std::vector<CompressionResult> results;
    if (samples.empty()) return results;

    // Collect values for each parameter
    std::map<ParameterType, std::vector<int32_t>> paramValues;
    for (auto& s : samples) {
        for (auto& kv : s.values) {
            paramValues[kv.first].push_back(scaleFloat(kv.second, scale));
        }
    }

    // Compress each parameter separately
    for (auto& kv : paramValues) {
        auto param = kv.first;
        auto& values = kv.second;

        CompressionResult res;
        res.param = param;
        res.method = "Delta";
        res.nSamples = values.size();
        res.originalSize = values.size() * sizeof(int32_t);

        auto t1 = std::chrono::high_resolution_clock::now();
        auto deltas = deltaEncode(values);
        auto t2 = std::chrono::high_resolution_clock::now();

        res.compressedSize = deltas.size() * sizeof(int32_t);
        res.ratio = (double)res.compressedSize / (double)res.originalSize;
        res.cpuTimeMs = std::chrono::duration<double, std::milli>(t2 - t1).count();

        // Verify
        auto valuesDecoded = deltaDecode(deltas);
        res.compressed_value = deltas;
        res.verified = (values == valuesDecoded);

        results.push_back(res);
    }

    return results;
}


// ---------------- High-level compression ----------------
std::vector<CompressionResult> compressBufferRle(const std::vector<Sample>& samples, int scale) {
    std::vector<CompressionResult> results;
    if (samples.empty()) return results;

    // Collect values for each parameter
    std::map<ParameterType, std::vector<int32_t>> paramValues;
    for (auto& s : samples) {
        for (auto& kv : s.values) {
            paramValues[kv.first].push_back(scaleFloat(kv.second, scale));
        }
    }

    // Compress each parameter separately
    for (auto& kv : paramValues) {
        auto param = kv.first;
        auto& values = kv.second;

        CompressionResult res;
        res.param = param;
        res.method = "RLE";
        res.nSamples = values.size();
        res.originalSize = values.size() * sizeof(int32_t);

        auto t1 = std::chrono::high_resolution_clock::now();
        auto rle = rleEncode(values);
        auto t2 = std::chrono::high_resolution_clock::now();

        res.compressedSize = rle.size() * 2 * sizeof(int32_t); // (val,count)
        res.ratio = (double)res.compressedSize / (double)res.originalSize;
        res.cpuTimeMs = std::chrono::duration<double, std::milli>(t2 - t1).count();

        // Verify
        auto valuesDecoded = rleDecode(rle);
        res.verified = (values == valuesDecoded);

        results.push_back(res);
    }

    return results;
}
