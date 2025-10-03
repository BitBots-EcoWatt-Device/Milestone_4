#ifndef ESP8266_DATA_TYPES_H
#define ESP8266_DATA_TYPES_H

#include <Arduino.h>
#include <vector>

enum class ParameterType : uint8_t
{
    AC_VOLTAGE = 0,
    AC_CURRENT = 1,
    AC_FREQUENCY = 2,
    PV1_VOLTAGE = 3,
    PV2_VOLTAGE = 4,
    PV1_CURRENT = 5,
    PV2_CURRENT = 6,
    TEMPERATURE = 7,
    EXPORT_POWER_PERCENT = 8,
    OUTPUT_POWER = 9
};

struct ParameterConfig
{
    String name;
    String unit;
};

struct Sample
{
    unsigned long timestamp;
    std::vector<std::pair<ParameterType, float>> values;

    void setValue(ParameterType param, float value)
    {
        for (auto &pair : values)
        {
            if (pair.first == param)
            {
                pair.second = value;
                return;
            }
        }
        values.push_back(std::make_pair(param, value));
    }

    bool hasValue(ParameterType param) const
    {
        for (const auto &pair : values)
        {
            if (pair.first == param)
                return true;
        }
        return false;
    }

    float getValue(ParameterType param) const
    {
        for (const auto &pair : values)
        {
            if (pair.first == param)
                return pair.second;
        }
        return 0.0f;
    }
};

class ESP8266DataBuffer
{
public:
    explicit ESP8266DataBuffer(size_t capacity);

    bool hasSpace() const { return buffer_.size() < capacity_; }
    void append(const Sample &sample);
    std::vector<Sample> flush();
    // New: non-destructive snapshot; caller can clear() on success
    std::vector<Sample> snapshot() const { return buffer_; }
    void clear() { buffer_.clear(); }
    size_t size() const { return buffer_.size(); }
    bool empty() const { return buffer_.empty(); }

private:
    std::vector<Sample> buffer_;
    size_t capacity_;
};

String parameterTypeToString(ParameterType param);
ParameterType stringToParameterType(const String &str);

#endif // ESP8266_DATA_TYPES_H