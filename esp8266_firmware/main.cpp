#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include "Inverter.h"
#include "PollingConfig.h"
#include "compress.h"
#include "packetizer.h"
#include <curl/curl.h>

// ================= Buffer ==================
class DataBuffer
{
public:
    explicit DataBuffer(size_t cap) : capacity_(cap) {}
    bool hasSpace()
    {
        std::lock_guard<std::mutex> l(m_);
        return buf_.size() < capacity_;
    }
    void append(const Sample &s)
    {
        std::lock_guard<std::mutex> l(m_);
        buf_.push_back(s);
    }
    std::vector<Sample> flush()
    {
        std::lock_guard<std::mutex> l(m_);
        auto out = buf_;
        buf_.clear();
        return out;
    }

private:
    std::vector<Sample> buf_;
    std::mutex m_;
    size_t capacity_;
};

// ================= Loops ==================
void pollLoop(Inverter &inverter, DataBuffer &buf,
              std::chrono::milliseconds pollInt, const PollingConfig &config)
{
    auto start = std::chrono::steady_clock::now();
    while (true)
    {
        Sample sample;
        sample.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();

        bool allSuccess = true;
        const auto &enabledParams = config.getEnabledParameters();

        for (auto paramType : enabledParams)
        {
            const auto &paramConfig = config.getParameterConfig(paramType);
            float value;

            if (paramConfig.readFunction(inverter, value))
            {
                sample.setValue(paramType, value);
            }
            else
            {
                std::cerr << "Failed to read " << paramConfig.name << std::endl;
                allSuccess = false;
            }
        }

        if (allSuccess && buf.hasSpace())
        {
            buf.append(sample);
        }
        else if (!allSuccess)
        {
            std::cerr << "Poll failed for some parameters\n";
        }

        std::this_thread::sleep_for(pollInt);
    }
}
void uploadLoop(DataBuffer &buf, std::chrono::milliseconds upInt, const PollingConfig &config)
{
    while (true)
    {
        std::this_thread::sleep_for(upInt);
        auto data = buf.flush();

        if (data.empty()) {
            std::cout << "Buffer is empty, nothing to upload.\n";
            return;
        }

        for (const auto& sample : data)
        {
            std::cout << "Uncompressed Sample - t=" << sample.timestamp << " ms";
            for (auto paramType : config.getEnabledParameters())
            {
            if (sample.hasValue(paramType))
            {
                const auto &paramConfig = config.getParameterConfig(paramType);
                std::cout << " " << paramConfig.name << "=" << sample.getValue(paramType)
                      << paramConfig.unit;
            }
            }
            std::cout << "\n";
        }

        for (const auto& sample : data)
        {
            std::vector<CompressionResult> compressed_samples_Delta = compressBufferDelta({sample});

            // Convert CompressionResult to CompressedFieldBinary for packetization
            std::vector<CompressedFieldBinary> fields_to_packet;
            
            for (const auto& result : compressed_samples_Delta)
            {
                CompressedFieldBinary field;
                field.param_id = static_cast<int>(result.param);
                field.param_name = to_string(result.param); // or use a mapping to names
                field.payload = result.compressed_value; // assuming std::vector<uint32_t>
                field.method = result.method;
                field.n_samples = result.nSamples; // if available, else 1
                field.cpu_time_ms = result.cpuTimeMs;
                fields_to_packet.push_back(field);

                std::cout << "Parameter: " << static_cast<int>(result.param)
                          << ", Method: " << result.method
                          << ", Samples: " << result.nSamples
                          << ", Original Size: " << result.originalSize
                          << ", Compressed Size: " << result.compressedSize
                          << ", Ratio: " << result.ratio
                          << ", CPU Time (ms): " << result.cpuTimeMs
                          << ", Verified: " << (result.verified ? "Yes" : "No")
                          << ", Compressed Values: [";
                for (size_t i = 0; i < result.compressed_value.size(); ++i) {
                    std::cout << result.compressed_value[i];
                    if (i != result.compressed_value.size() - 1)
                        std::cout << ", ";
                }
                std::cout << "]\n";
            }
            // Build the packet JSON
            // Ensure build_meta_json serializes payload as a JSON array of numbers
            std::string packet_compressed_samples = build_meta_json("002", sample.timestamp, fields_to_packet);
            std::cout << "Packet JSON: " << packet_compressed_samples << "\n";

            // Optionally, you can now upload using your upload_multipart function
            // UploadResult res = upload_multipart(server_url, packet_compressed_samples, fields_to_packet, max_chunk_bytes, max_retries, timeout_seconds);

            // Send packet_compressed_samples as JSON to the Flask server
            CURL *curl = curl_easy_init();
            if (curl) {
                CURLcode res;
                struct curl_slist *headers = nullptr;
                headers = curl_slist_append(headers, "Content-Type: application/json");

                curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.3:5000/upload"); // adjust endpoint as needed
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, packet_compressed_samples.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, packet_compressed_samples.size());

                res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
                } else {
                    std::cout << "Packet sent to server successfully.\n";
                }

                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
            }
        }

    }
}

// ================= Main ==================
int main()
{
    std::cout << "=== Inverter Communication Demo ===\n";

    // Create Inverter instance (configuration loaded automatically)
    Inverter inverter;

    // Demo: write once
    if (inverter.setExportPowerPercent(20))
    {
        std::cout << "Export power set to 20%\n";
    }
    else
    {
        std::cerr << "Failed to set export power percent\n";
    }

    // Demo: dynamic register read (temperature and export power percent)
    float temperature;
    int exportPercent;
    if (inverter.getTemperature(temperature) && inverter.getExportPowerPercent(exportPercent))
    {
        std::cout << "Temperature: " << temperature << " C\n";
        std::cout << "Export Power Percent: " << exportPercent << " %\n";
    }
    else
    {
        std::cerr << "Failed to read temperature and export power percent\n";
    }

    // Demo: comprehensive AC measurements
    float acVoltage, acCurrent, acFrequency;
    if (inverter.getACMeasurements(acVoltage, acCurrent, acFrequency))
    {
        std::cout << "AC Measurements - Voltage: " << acVoltage << " V, Current: " << acCurrent << " A, Frequency: " << acFrequency << " Hz\n";
    }
    else
    {
        std::cerr << "Failed to read AC measurements\n";
    }

    // Demo: PV input measurements
    float pv1Voltage, pv2Voltage, pv1Current, pv2Current;
    if (inverter.getPVMeasurements(pv1Voltage, pv2Voltage, pv1Current, pv2Current))
    {
        std::cout << "PV1 - Voltage: " << pv1Voltage << " V, Current: " << pv1Current << " A\n";
        std::cout << "PV2 - Voltage: " << pv2Voltage << " V, Current: " << pv2Current << " A\n";
    }
    else
    {
        std::cerr << "Failed to read PV measurements\n";
    }

    // Demo: system status
    int outputPower;
    if (inverter.getSystemStatus(temperature, exportPercent, outputPower))
    {
        std::cout << "System Status - Temperature: " << temperature << " C, Export: " << exportPercent << " %, Output Power: " << outputPower << " W\n";
    }
    else
    {
        std::cerr << "Failed to read system status\n";
    }

    // Demo: dynamic register read (voltage and current)
    float voltage, current;
    if (inverter.getACVoltage(voltage) && inverter.getACCurrent(current))
    {
        std::cout << "[Dynamic] Voltage: " << voltage << " V\n";
        std::cout << "[Dynamic] Current: " << current << " A\n";
    }
    else
    {
        std::cerr << "Failed to read voltage and current registers dynamically\n";
    }

    // ================= Dynamic Polling Configuration Demo ===================
    std::cout << "\n=== Dynamic Polling Configuration ===\n";

    // Create and configure polling parameters
    PollingConfig pollingConfig;

    // Configure to poll AC Voltage and Current only
    std::cout << "\nConfiguring to poll AC voltage and AC current...\n";
    pollingConfig.setParameters({ParameterType::AC_VOLTAGE, ParameterType::AC_CURRENT, ParameterType::AC_FREQUENCY});
    pollingConfig.printEnabledParameters();

    // Start polling with the configured parameters
    std::cout << "\n=== Starting Dynamic Polling ===\n";

    DataBuffer buffer(30);
    std::thread pollT(pollLoop, std::ref(inverter), std::ref(buffer),
                      std::chrono::milliseconds(5000), std::ref(pollingConfig));
    std::thread upT(uploadLoop, std::ref(buffer), std::chrono::milliseconds(30000),
                    std::ref(pollingConfig));
    pollT.join();
    upT.join();
    return 0;
}
