#include "ESP8266Inverter.h"
#include "ESP8266Parameters.h"

ESP8266Inverter::ESP8266Inverter() : slaveAddress_(0x11)
{
}

bool ESP8266Inverter::begin()
{
    return modbusHandler_.begin();
}

void ESP8266Inverter::setSlaveAddress(uint8_t slaveAddr)
{
    slaveAddress_ = slaveAddr;
}

bool ESP8266Inverter::read(ParameterType id, float &out)
{
    const ParamDesc *param = find_param(id);
    if (!param)
    {
        return false;
    }

    uint16_t reg_addr = pgm_read_word(&param->reg);
    float scale = pgm_read_float(&param->scale);

    uint16_t raw_value;
    if (readSingleRegister(reg_addr, raw_value))
    {
        out = raw_value / scale;
        return true;
    }
    return false;
}

bool ESP8266Inverter::getACVoltage(float &voltage)
{
    return read(ParameterType::AC_VOLTAGE, voltage);
}

bool ESP8266Inverter::getACCurrent(float &current)
{
    return read(ParameterType::AC_CURRENT, current);
}

bool ESP8266Inverter::getACFrequency(float &frequency)
{
    return read(ParameterType::AC_FREQUENCY, frequency);
}

bool ESP8266Inverter::getPV1Voltage(float &voltage)
{
    return read(ParameterType::PV1_VOLTAGE, voltage);
}

bool ESP8266Inverter::getPV2Voltage(float &voltage)
{
    return read(ParameterType::PV2_VOLTAGE, voltage);
}

bool ESP8266Inverter::getPV1Current(float &current)
{
    return read(ParameterType::PV1_CURRENT, current);
}

bool ESP8266Inverter::getPV2Current(float &current)
{
    return read(ParameterType::PV2_CURRENT, current);
}

bool ESP8266Inverter::getTemperature(float &temperature)
{
    return read(ParameterType::TEMPERATURE, temperature);
}

bool ESP8266Inverter::getExportPowerPercent(int &exportPercent)
{
    float value;
    if (read(ParameterType::EXPORT_POWER_PERCENT, value))
    {
        exportPercent = static_cast<int>(value);
        return true;
    }
    return false;
}

bool ESP8266Inverter::getOutputPower(int &power)
{
    float value;
    if (read(ParameterType::OUTPUT_POWER, value))
    {
        power = static_cast<int>(value);
        return true;
    }
    return false;
}

bool ESP8266Inverter::getACMeasurements(float &voltage, float &current, float &frequency)
{
    std::vector<uint16_t> values;
    if (modbusHandler_.readRegisters(REG_AC_VOLTAGE, 3, values, slaveAddress_) && values.size() >= 3)
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
    if (modbusHandler_.readRegisters(REG_PV1_VOLTAGE, 4, values, slaveAddress_) && values.size() >= 4)
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
    if (modbusHandler_.readRegisters(REG_TEMPERATURE, 3, values, slaveAddress_) && values.size() >= 3)
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
    if (modbusHandler_.readRegisters(regAddr, 1, values, slaveAddress_) && !values.empty())
    {
        value = values[0];
        return true;
    }
    return false;
}

bool ESP8266Inverter::writeSingleRegister(uint16_t regAddr, uint16_t value)
{
    return modbusHandler_.writeRegister(regAddr, value, slaveAddress_);
}