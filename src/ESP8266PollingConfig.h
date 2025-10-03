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

    // Get parameter name and unit from descriptor table
    String getParameterName(ParameterType param) const;
    String getParameterUnit(ParameterType param) const;

    void printEnabledParameters() const;
    bool isParameterEnabled(ParameterType param) const;

private:
    std::vector<ParameterType> enabledParameters_;
};

#endif // ESP8266_POLLING_CONFIG_H