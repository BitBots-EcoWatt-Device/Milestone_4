#include "ESP8266Parameters.h"
#include <string.h>

// Parameter descriptor table (stored in RAM as simple const data)
const ParamDesc kParams[] = {
    {ParameterType::AC_VOLTAGE, "AC Voltage", "V", 0, 10.0f},
    {ParameterType::AC_CURRENT, "AC Current", "A", 1, 10.0f},
    {ParameterType::AC_FREQUENCY, "AC Frequency", "Hz", 2, 100.0f},
    {ParameterType::PV1_VOLTAGE, "PV1 Voltage", "V", 3, 10.0f},
    {ParameterType::PV2_VOLTAGE, "PV2 Voltage", "V", 4, 10.0f},
    {ParameterType::PV1_CURRENT, "PV1 Current", "A", 5, 10.0f},
    {ParameterType::PV2_CURRENT, "PV2 Current", "A", 6, 10.0f},
    {ParameterType::TEMPERATURE, "Temperature", "°C", 7, 10.0f},
    {ParameterType::EXPORT_POWER_PERCENT, "Export Power Percent", "%", 8, 1.0f},
    {ParameterType::OUTPUT_POWER, "Output Power", "W", 9, 1.0f}};

const size_t kParamsCount = sizeof(kParams) / sizeof(kParams[0]);

const ParamDesc *find_param(ParameterType id)
{
    for (size_t i = 0; i < kParamsCount; i++)
    {
        if (kParams[i].id == id)
        {
            return &kParams[i];
        }
    }
    return nullptr;
}

const ParamDesc *find_param_by_name(const char *name)
{
    if (!name)
        return nullptr;

    for (size_t i = 0; i < kParamsCount; i++)
    {
        if (strcmp(name, kParams[i].name) == 0)
        {
            return &kParams[i];
        }
    }
    return nullptr;
}