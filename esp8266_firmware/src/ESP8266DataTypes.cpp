#include "ESP8266DataTypes.h"
#include "ESP8266Inverter.h"

ESP8266DataBuffer::ESP8266DataBuffer(size_t capacity) : capacity_(capacity)
{
    buffer_.reserve(capacity);
}

void ESP8266DataBuffer::append(const Sample &sample)
{
    if (buffer_.size() >= capacity_)
    {
        // Remove oldest sample if buffer is full
        buffer_.erase(buffer_.begin());
    }
    buffer_.push_back(sample);
}

std::vector<Sample> ESP8266DataBuffer::flush()
{
    std::vector<Sample> result = buffer_;
    buffer_.clear();
    return result;
}

String parameterTypeToString(ParameterType param)
{
    switch (param)
    {
    case ParameterType::AC_VOLTAGE:
        return "AC_VOLTAGE";
    case ParameterType::AC_CURRENT:
        return "AC_CURRENT";
    case ParameterType::AC_FREQUENCY:
        return "AC_FREQUENCY";
    case ParameterType::PV1_VOLTAGE:
        return "PV1_VOLTAGE";
    case ParameterType::PV2_VOLTAGE:
        return "PV2_VOLTAGE";
    case ParameterType::PV1_CURRENT:
        return "PV1_CURRENT";
    case ParameterType::PV2_CURRENT:
        return "PV2_CURRENT";
    case ParameterType::TEMPERATURE:
        return "TEMPERATURE";
    case ParameterType::EXPORT_POWER_PERCENT:
        return "EXPORT_POWER_PERCENT";
    case ParameterType::OUTPUT_POWER:
        return "OUTPUT_POWER";
    default:
        return "UNKNOWN";
    }
}

ParameterType stringToParameterType(const String &str)
{
    if (str == "AC_VOLTAGE")
        return ParameterType::AC_VOLTAGE;
    if (str == "AC_CURRENT")
        return ParameterType::AC_CURRENT;
    if (str == "AC_FREQUENCY")
        return ParameterType::AC_FREQUENCY;
    if (str == "PV1_VOLTAGE")
        return ParameterType::PV1_VOLTAGE;
    if (str == "PV2_VOLTAGE")
        return ParameterType::PV2_VOLTAGE;
    if (str == "PV1_CURRENT")
        return ParameterType::PV1_CURRENT;
    if (str == "PV2_CURRENT")
        return ParameterType::PV2_CURRENT;
    if (str == "TEMPERATURE")
        return ParameterType::TEMPERATURE;
    if (str == "EXPORT_POWER_PERCENT")
        return ParameterType::EXPORT_POWER_PERCENT;
    if (str == "OUTPUT_POWER")
        return ParameterType::OUTPUT_POWER;
    return ParameterType::AC_VOLTAGE; // Default
}