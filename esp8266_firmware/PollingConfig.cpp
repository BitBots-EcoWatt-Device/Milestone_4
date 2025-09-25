#include "PollingConfig.h"
#include "Inverter.h"
#include <iostream>

PollingConfig::PollingConfig()
{
    initializeParameterConfigs();
    // Default polling configuration (voltage and current for backward compatibility)
    enabledParams_ = {ParameterType::AC_VOLTAGE, ParameterType::AC_CURRENT};
}

void PollingConfig::initializeParameterConfigs()
{
    // Initialize available parameters
    availableParams_[ParameterType::AC_VOLTAGE] = ParameterConfig(
        ParameterType::AC_VOLTAGE, "AC_Voltage", "V",
        [](Inverter &inv, float &val)
        { return inv.getACVoltage(val); });

    availableParams_[ParameterType::AC_CURRENT] = ParameterConfig(
        ParameterType::AC_CURRENT, "AC_Current", "A",
        [](Inverter &inv, float &val)
        { return inv.getACCurrent(val); });

    availableParams_[ParameterType::AC_FREQUENCY] = ParameterConfig(
        ParameterType::AC_FREQUENCY, "AC_Frequency", "Hz",
        [](Inverter &inv, float &val)
        { return inv.getACFrequency(val); });

    availableParams_[ParameterType::PV1_VOLTAGE] = ParameterConfig(
        ParameterType::PV1_VOLTAGE, "PV1_Voltage", "V",
        [](Inverter &inv, float &val)
        { return inv.getPV1Voltage(val); });

    availableParams_[ParameterType::PV2_VOLTAGE] = ParameterConfig(
        ParameterType::PV2_VOLTAGE, "PV2_Voltage", "V",
        [](Inverter &inv, float &val)
        { return inv.getPV2Voltage(val); });

    availableParams_[ParameterType::PV1_CURRENT] = ParameterConfig(
        ParameterType::PV1_CURRENT, "PV1_Current", "A",
        [](Inverter &inv, float &val)
        { return inv.getPV1Current(val); });

    availableParams_[ParameterType::PV2_CURRENT] = ParameterConfig(
        ParameterType::PV2_CURRENT, "PV2_Current", "A",
        [](Inverter &inv, float &val)
        { return inv.getPV2Current(val); });

    availableParams_[ParameterType::TEMPERATURE] = ParameterConfig(
        ParameterType::TEMPERATURE, "Temperature", "°C",
        [](Inverter &inv, float &val)
        { return inv.getTemperature(val); });

    availableParams_[ParameterType::EXPORT_POWER_PERCENT] = ParameterConfig(
        ParameterType::EXPORT_POWER_PERCENT, "Export_Power_Percent", "%",
        [](Inverter &inv, float &val)
        {
            int intVal;
            bool result = inv.getExportPowerPercent(intVal);
            val = static_cast<float>(intVal);
            return result;
        });

    availableParams_[ParameterType::OUTPUT_POWER] = ParameterConfig(
        ParameterType::OUTPUT_POWER, "Output_Power", "W",
        [](Inverter &inv, float &val)
        {
            int intVal;
            bool result = inv.getOutputPower(intVal);
            val = static_cast<float>(intVal);
            return result;
        });
}

void PollingConfig::addParameter(ParameterType param)
{
    if (availableParams_.find(param) != availableParams_.end())
    {
        enabledParams_.insert(param);
    }
}

void PollingConfig::removeParameter(ParameterType param)
{
    enabledParams_.erase(param);
}

void PollingConfig::setParameters(const std::vector<ParameterType> &params)
{
    enabledParams_.clear();
    for (auto param : params)
    {
        addParameter(param);
    }
}

const std::set<ParameterType> &PollingConfig::getEnabledParameters() const
{
    return enabledParams_;
}

const ParameterConfig &PollingConfig::getParameterConfig(ParameterType param) const
{
    return availableParams_.at(param);
}

void PollingConfig::printEnabledParameters() const
{
    std::cout << "Enabled polling parameters:\n";
    for (auto param : enabledParams_)
    {
        const auto &config = availableParams_.at(param);
        std::cout << "  - " << config.name << " (" << config.unit << ")\n";
    }
}

// Predefined monitoring profiles for common use cases
void PollingConfig::setBasicACProfile()
{
    setParameters({ParameterType::AC_VOLTAGE,
                   ParameterType::AC_CURRENT,
                   ParameterType::AC_FREQUENCY});
}

void PollingConfig::setComprehensiveProfile()
{
    setParameters({ParameterType::AC_VOLTAGE,
                   ParameterType::AC_CURRENT,
                   ParameterType::AC_FREQUENCY,
                   ParameterType::TEMPERATURE,
                   ParameterType::OUTPUT_POWER,
                   ParameterType::EXPORT_POWER_PERCENT});
}

void PollingConfig::setPVMonitoringProfile()
{
    setParameters({ParameterType::PV1_VOLTAGE,
                   ParameterType::PV1_CURRENT,
                   ParameterType::PV2_VOLTAGE,
                   ParameterType::PV2_CURRENT,
                   ParameterType::TEMPERATURE});
}

void PollingConfig::setThermalProfile()
{
    setParameters({ParameterType::TEMPERATURE,
                   ParameterType::OUTPUT_POWER});
}

// ================= Sample Implementation ==================
void Sample::setValue(ParameterType param, float value)
{
    values[param] = value;
}

float Sample::getValue(ParameterType param) const
{
    auto it = values.find(param);
    return (it != values.end()) ? it->second : 0.0f;
}

bool Sample::hasValue(ParameterType param) const
{
    return values.find(param) != values.end();
}
