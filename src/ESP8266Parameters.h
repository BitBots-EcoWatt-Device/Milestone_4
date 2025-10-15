#ifndef ESP8266_PARAMETERS_H
#define ESP8266_PARAMETERS_H

#include <Arduino.h>
#include "ESP8266DataTypes.h"

// Parameter descriptor structure
struct ParamDesc
{
    ParameterType id;
    const char *name;
    const char *unit;
    uint16_t reg;
    float scale;
};

// Global parameter descriptor table
extern const ParamDesc kParams[];
extern const size_t kParamsCount;

// Helper functions
const ParamDesc *find_param(ParameterType id);
const ParamDesc *find_param_by_name(const char *name);

#endif // ESP8266_PARAMETERS_H