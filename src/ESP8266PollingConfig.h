#ifndef ESP8266_POLLING_CONFIG_H
#define ESP8266_POLLING_CONFIG_H

#include <Arduino.h>
#include <vector>
#include "ESP8266DataTypes.h"

class ESP8266Inverter; // Forward declaration

class ESP8266PollingConfig
{
public:
    ESP8266PollingConfig();

    void setParameters(const std::vector<ParameterType> &params);
    const std::vector<ParameterType> &getEnabledParameters() const { return enabledParameters_; }

    const ParameterConfig &getParameterConfig(ParameterType param) const;
    void printEnabledParameters() const;

    bool isParameterEnabled(ParameterType param) const;

private:
    std::vector<ParameterType> enabledParameters_;
    void initializeParameterConfigs();

    static ParameterConfig parameterConfigs_[10]; // Static array for all parameter configs
    static bool configsInitialized_;
};

// Helper functions for parameter reading
namespace ParameterReaders
{
    bool readACVoltage(ESP8266Inverter &inverter, float &value);
    bool readACCurrent(ESP8266Inverter &inverter, float &value);
    bool readACFrequency(ESP8266Inverter &inverter, float &value);
    bool readPV1Voltage(ESP8266Inverter &inverter, float &value);
    bool readPV2Voltage(ESP8266Inverter &inverter, float &value);
    bool readPV1Current(ESP8266Inverter &inverter, float &value);
    bool readPV2Current(ESP8266Inverter &inverter, float &value);
    bool readTemperature(ESP8266Inverter &inverter, float &value);
    bool readExportPowerPercent(ESP8266Inverter &inverter, float &value);
    bool readOutputPower(ESP8266Inverter &inverter, float &value);
}

#endif // ESP8266_POLLING_CONFIG_H