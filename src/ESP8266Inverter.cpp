#include "ESP8266Inverter.h"

ESP8266Inverter::ESP8266Inverter()
{
}

bool ESP8266Inverter::begin()
{
    return modbusHandler_.begin();
}

bool ESP8266Inverter::getACVoltage(float &voltage)
{
    uint16_t value;
    if (readSingleRegister(REG_AC_VOLTAGE, value))
    {
        voltage = value / 10.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getACCurrent(float &current)
{
    uint16_t value;
    if (readSingleRegister(REG_AC_CURRENT, value))
    {
        current = value / 10.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getACFrequency(float &frequency)
{
    uint16_t value;
    if (readSingleRegister(REG_AC_FREQUENCY, value))
    {
        frequency = value / 100.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getPV1Voltage(float &voltage)
{
    uint16_t value;
    if (readSingleRegister(REG_PV1_VOLTAGE, value))
    {
        voltage = value / 10.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getPV2Voltage(float &voltage)
{
    uint16_t value;
    if (readSingleRegister(REG_PV2_VOLTAGE, value))
    {
        voltage = value / 10.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getPV1Current(float &current)
{
    uint16_t value;
    if (readSingleRegister(REG_PV1_CURRENT, value))
    {
        current = value / 10.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getPV2Current(float &current)
{
    uint16_t value;
    if (readSingleRegister(REG_PV2_CURRENT, value))
    {
        current = value / 10.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getTemperature(float &temperature)
{
    uint16_t value;
    if (readSingleRegister(REG_TEMPERATURE, value))
    {
        temperature = value / 10.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getExportPowerPercent(int &exportPercent)
{
    uint16_t value;
    if (readSingleRegister(REG_EXPORT_POWER_PERCENT, value))
    {
        exportPercent = value;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getOutputPower(int &power)
{
    uint16_t value;
    if (readSingleRegister(REG_OUTPUT_POWER, value))
    {
        power = value;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getACMeasurements(float &voltage, float &current, float &frequency)
{
    std::vector<uint16_t> values;
    if (modbusHandler_.readRegisters(REG_AC_VOLTAGE, 3, values, SLAVE_ADDRESS) && values.size() >= 3)
    {
        voltage = values[0] / 10.0f;
        current = values[1] / 10.0f;
        frequency = values[2] / 100.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getPVMeasurements(float &pv1Voltage, float &pv2Voltage, float &pv1Current, float &pv2Current)
{
    std::vector<uint16_t> values;
    if (modbusHandler_.readRegisters(REG_PV1_VOLTAGE, 4, values, SLAVE_ADDRESS) && values.size() >= 4)
    {
        pv1Voltage = values[0] / 10.0f;
        pv2Voltage = values[1] / 10.0f;
        pv1Current = values[2] / 10.0f;
        pv2Current = values[3] / 10.0f;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getSystemStatus(float &temperature, int &exportPercent, int &outputPower)
{
    std::vector<uint16_t> values;
    if (modbusHandler_.readRegisters(REG_TEMPERATURE, 3, values, SLAVE_ADDRESS) && values.size() >= 3)
    {
        temperature = values[0] / 10.0f;
        exportPercent = values[1];
        outputPower = values[2];
        return true;
    }
    return false;
}

bool ESP8266Inverter::setExportPowerPercent(int value)
{
    return writeSingleRegister(REG_EXPORT_POWER_PERCENT, static_cast<uint16_t>(value));
}

bool ESP8266Inverter::readSingleRegister(uint16_t regAddr, uint16_t &value)
{
    std::vector<uint16_t> values;
    if (modbusHandler_.readRegisters(regAddr, 1, values, SLAVE_ADDRESS) && !values.empty())
    {
        value = values[0];
        return true;
    }
    return false;
}

bool ESP8266Inverter::writeSingleRegister(uint16_t regAddr, uint16_t value)
{
    return modbusHandler_.writeRegister(regAddr, value, SLAVE_ADDRESS);
}