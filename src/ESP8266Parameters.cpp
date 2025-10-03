#include "ESP8266Parameters.h"
#include <string.h>

// Parameter descriptor table stored in flash memory
const ParamDesc kParams[] PROGMEM = {
    {ParameterType::AC_VOLTAGE, PSTR("AC Voltage"), PSTR("V"), 0, 10.0f},
    {ParameterType::AC_CURRENT, PSTR("AC Current"), PSTR("A"), 1, 10.0f},
    {ParameterType::AC_FREQUENCY, PSTR("AC Frequency"), PSTR("Hz"), 2, 100.0f},
    {ParameterType::PV1_VOLTAGE, PSTR("PV1 Voltage"), PSTR("V"), 3, 10.0f},
    {ParameterType::PV2_VOLTAGE, PSTR("PV2 Voltage"), PSTR("V"), 4, 10.0f},
    {ParameterType::PV1_CURRENT, PSTR("PV1 Current"), PSTR("A"), 5, 10.0f},
    {ParameterType::PV2_CURRENT, PSTR("PV2 Current"), PSTR("A"), 6, 10.0f},
    {ParameterType::TEMPERATURE, PSTR("Temperature"), PSTR("°C"), 7, 10.0f},
    {ParameterType::EXPORT_POWER_PERCENT, PSTR("Export Power Percent"), PSTR("%"), 8, 1.0f},
    {ParameterType::OUTPUT_POWER, PSTR("Output Power"), PSTR("W"), 9, 1.0f}};

const size_t kParamsCount = sizeof(kParams) / sizeof(kParams[0]);

const ParamDesc *find_param(ParameterType id)
{
    for (size_t i = 0; i < kParamsCount; i++)
    {
        ParameterType param_id = static_cast<ParameterType>(pgm_read_byte(&kParams[i].id));
        if (param_id == id)
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
        // Read name pointer from PROGMEM and compare
        const char *param_name = reinterpret_cast<const char *>(pgm_read_ptr(&kParams[i].name));
        if (strcmp_P(name, param_name) == 0)
        {
            return &kParams[i];
        }
    }
    return nullptr;
}