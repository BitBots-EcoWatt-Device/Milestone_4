#include "packetizer.h"
#include <sstream>
#include <curl/curl.h>
#include <iomanip>
#include <thread>
#include <chrono>

// If you don't have nlohmann/json, the build_meta_json below uses manual JSON strings.
// If you have it, uncomment the JSON version for safer encoding.
// using json = nlohmann::json;

static std::string escape_json_string(const std::string& s) {
    std::ostringstream o;
    for (auto c : s) {
        switch (c) {
        case '\"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if ((unsigned char)c < 0x20) {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            } else {
                o << c;
            }
        }
    }
    return o.str();
}

std::string build_meta_json(const std::string& device_id, long long timestamp,
                            const std::vector<CompressedFieldBinary>& fields) {
    // Simple JSON builder (safe enough for our fields)
    std::ostringstream ss;
    ss << "{";
    ss << "\"device_id\":\"" << escape_json_string(device_id) << "\",";
    ss << "\"timestamp\":" << timestamp << ",";
    ss << "\"fields\":{";
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto &f = fields[i];
        ss << "\"" << f.param_name << "\":{";
        ss << "\"method\":\"" << f.method << "\",";
        ss << "\"param_id\":" << f.param_id << ",";
        ss << "\"n_samples\":" << f.n_samples << ",";
        ss << "\"bytes_len\":" << f.payload.size() << ",";
        ss << "\"cpu_time_ms\":" << f.cpu_time_ms << ",";
        // Add payload as array of integers
        ss << "\"payload\":[";
        for (size_t j = 0; j < f.payload.size(); ++j) {
            ss << f.payload[j];
            if (j + 1 < f.payload.size()) ss << ",";
        }
        ss << "]";
        ss << "}";
        if (i + 1 < fields.size()) ss << ",";
    }
    ss << "}";
    ss << "}";
    // compute placeholder hmac? Keep meta without hmac; compute separately if needed
    return ss.str();
}

std::string compute_placeholder_hmac(const std::string& meta_json) {
    // Very small stub: compute simple checksum hex string. Replace with HMAC-SHA256 in production.
    uint32_t acc = 2166136261u;
    for (unsigned char c : meta_json) {
        acc ^= c;
        acc *= 16777619u;
    }
    std::ostringstream o;
    o << std::hex << std::setw(8) << std::setfill('0') << (acc & 0xffffffffu);
    return o.str();
}

// helper for libcurl write callback (collect server response)
static size_t curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* s = reinterpret_cast<std::string*>(userdata);
    size_t total = size * nmemb;
    s->append(reinterpret_cast<char*>(ptr), total);
    return total;
}

// Build a CURL mime/form with meta + files (supports chunk splitting)
static curl_mime* build_curl_mime(CURL* curl, const std::string& meta_json, const std::vector<CompressedFieldBinary>& fields, size_t max_chunk_bytes) {
    curl_mime* mime = curl_mime_init(curl);
    // meta field
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "meta");
    curl_mime_data(part, meta_json.c_str(), CURL_ZERO_TERMINATED);

    // hmac field (placeholder)
    std::string hmac = compute_placeholder_hmac(meta_json);
    curl_mimepart* hpart = curl_mime_addpart(mime);
    curl_mime_name(hpart, "meta_hmac");
    curl_mime_data(hpart, hmac.c_str(), CURL_ZERO_TERMINATED);

    // add binary parts
    for (const auto &f : fields) {
        size_t total = f.payload.size();
        if (total == 0) continue;
        if (total <= max_chunk_bytes) {
            // single file part
            curl_mimepart* p = curl_mime_addpart(mime);
            std::string name = f.param_name; // field name
            curl_mime_name(p, name.c_str());
            // provide filename
            std::string filename = name + ".bin";
            curl_mime_filename(p, filename.c_str());
            curl_mime_data(p, reinterpret_cast<const char*>(f.payload.data()), f.payload.size());
            curl_mime_type(p, "application/octet-stream");
        } else {
            // split into parts
            size_t idx = 0;
            int partno = 0;
            while (idx < total) {
                size_t take = std::min(max_chunk_bytes, total - idx);
                curl_mimepart* p = curl_mime_addpart(mime);
                std::ostringstream nm; nm << f.param_name << ".part" << partno;
                std::ostringstream fn; fn << f.param_name << ".part" << partno << ".bin";
                curl_mime_name(p, nm.str().c_str());
                curl_mime_filename(p, fn.str().c_str());
                // need to provide pointer into memory — libcurl copies the data, so use curl_mime_data
                curl_mime_data(p, reinterpret_cast<const char*>(f.payload.data() + idx), take);
                curl_mime_type(p, "application/octet-stream");
                idx += take;
                ++partno;
            }
        }
    }
    return mime;
}

UploadResult upload_multipart(const std::string& server_url,
                              const std::string& meta_json,
                              const std::vector<CompressedFieldBinary>& fields,
                              size_t max_chunk_bytes,
                              int max_retries,
                              long timeout_seconds) {
    UploadResult res;
    res.ok = false;
    res.http_code = 0;
    res.server_response = "";

    CURL* curl = curl_easy_init();
    if (!curl) {
        res.server_response = "curl init failed";
        return res;
    }

    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);

        // build mime
        curl_mime* mime = build_curl_mime(curl, meta_json, fields, max_chunk_bytes);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        // collect response
        std::string response_buf;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);

        // perform
        CURLcode code = curl_easy_perform(curl);
        long http_code = 0;
        if (code == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            res.http_code = static_cast<int>(http_code);
            res.server_response = response_buf;
            if (http_code == 200) {
                res.ok = true;
                curl_mime_free(mime);
                break;
            }
        } else {
            res.server_response = std::string("curl error: ") + curl_easy_strerror(code);
            res.ok = false;
        }

        curl_mime_free(mime);
        // backoff before next retry
        std::this_thread::sleep_for(std::chrono::seconds(1 + attempt));
    }

    curl_easy_cleanup(curl);
    return res;
}
