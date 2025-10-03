#include "ESP8266PollingConfig.h"
#include "ESP8266Inverter.h"
#include "ESP8266Parameters.h"

ESP8266PollingConfig::ESP8266PollingConfig()
{
}

void ESP8266PollingConfig::setParameters(const std::vector<ParameterType> &params)
{
    enabledParameters_ = params;
}

String ESP8266PollingConfig::getParameterName(ParameterType param) const
{
    const ParamDesc *desc = find_param(param);
    if (desc)
    {
        const char *name_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&desc->name));
        return String(reinterpret_cast<const __FlashStringHelper *>(name_ptr));
    }
    return String("");
}

String ESP8266PollingConfig::getParameterUnit(ParameterType param) const
{
    const ParamDesc *desc = find_param(param);
    if (desc)
    {
        const char *unit_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&desc->unit));
        return String(reinterpret_cast<const __FlashStringHelper *>(unit_ptr));
    }
    return String("");
}

void ESP8266PollingConfig::printEnabledParameters() const
{
    Serial.println("[POLLING] Enabled parameters:");
    for (ParameterType param : enabledParameters_)
    {
        String name = getParameterName(param);
        String unit = getParameterUnit(param);
        Serial.print("  - ");
        Serial.print(name);
        Serial.println(unit);
    }
}

bool ESP8266PollingConfig::isParameterEnabled(ParameterType param) const
{
    for (ParameterType enabledParam : enabledParameters_)
    {
        if (enabledParam == param)
            return true;
    }
    return false;
}