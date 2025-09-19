#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// Compressed field container: binary payload for a parameter
struct CompressedFieldBinary {
    int param_id;                 // ParameterType as integer
    std::string param_name;       // human readable (optional)
    std::vector<int32_t> payload; // compressed bytes (delta+rle serialized)
    std::string method;           // "delta+rle"
    size_t n_samples;
    double cpu_time_ms;
};

// Result from upload
struct UploadResult {
    bool ok;
    int http_code;
    std::string server_response;
};

// Build meta JSON (device_id, timestamp, fields metadata + hmac placeholder)
std::string build_meta_json(const std::string& device_id, long long timestamp,
                            const std::vector<CompressedFieldBinary>& fields);

// Compute placeholder HMAC (stub). Replace with real HMAC later.
std::string compute_placeholder_hmac(const std::string& meta_json);

// Perform multipart/form-data upload with retry and optional chunk-splitting.
// server_url e.g. "http://127.0.0.1:5000/upload"
UploadResult upload_multipart(const std::string& server_url,
                              const std::string& meta_json,
                              const std::vector<CompressedFieldBinary>& fields,
                              size_t max_chunk_bytes = 64*1024,
                              int max_retries = 3,
                              long timeout_seconds = 10);
