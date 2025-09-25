#include "Inverter.h"
#include <iostream>

Inverter::Inverter() : modbusHandler_() {}

// Individual register read operations
bool Inverter::getACVoltage(float &voltage)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_AC_VOLTAGE, 1, values, SLAVE_ADDRESS))
        return false;
    voltage = values[0] / GAIN_10;
    return true;
}

bool Inverter::getACCurrent(float &current)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_AC_CURRENT, 1, values, SLAVE_ADDRESS))
        return false;
    current = values[0] / GAIN_10;
    return true;
}

bool Inverter::getACFrequency(float &frequency)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_AC_FREQUENCY, 1, values, SLAVE_ADDRESS))
        return false;
    frequency = values[0] / GAIN_100;
    return true;
}

bool Inverter::getPV1Voltage(float &voltage)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_PV1_VOLTAGE, 1, values, SLAVE_ADDRESS))
        return false;
    voltage = values[0] / GAIN_10;
    return true;
}

bool Inverter::getPV2Voltage(float &voltage)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_PV2_VOLTAGE, 1, values, SLAVE_ADDRESS))
        return false;
    voltage = values[0] / GAIN_10;
    return true;
}

bool Inverter::getPV1Current(float &current)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_PV1_CURRENT, 1, values, SLAVE_ADDRESS))
        return false;
    current = values[0] / GAIN_10;
    return true;
}

bool Inverter::getPV2Current(float &current)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_PV2_CURRENT, 1, values, SLAVE_ADDRESS))
        return false;
    current = values[0] / GAIN_10;
    return true;
}

bool Inverter::getTemperature(float &temperature)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_TEMPERATURE, 1, values, SLAVE_ADDRESS))
        return false;
    temperature = values[0] / GAIN_10;
    return true;
}

bool Inverter::getExportPowerPercent(int &exportPercent)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_EXPORT_POWER_PERCENT, 1, values, SLAVE_ADDRESS))
        return false;
    exportPercent = static_cast<int>(values[0] / GAIN_1);
    return true;
}

bool Inverter::getOutputPower(int &power)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_OUTPUT_POWER, 1, values, SLAVE_ADDRESS))
        return false;
    power = static_cast<int>(values[0] / GAIN_1);
    return true;
}

// Combined read operations for efficiency
bool Inverter::getACMeasurements(float &voltage, float &current, float &frequency)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_AC_VOLTAGE, 3, values, SLAVE_ADDRESS))
        return false;
    voltage = values[0] / GAIN_10;
    current = values[1] / GAIN_10;
    frequency = values[2] / GAIN_100;
    return true;
}

bool Inverter::getPVMeasurements(float &pv1Voltage, float &pv2Voltage, float &pv1Current, float &pv2Current)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_PV1_VOLTAGE, 4, values, SLAVE_ADDRESS))
        return false;
    pv1Voltage = values[0] / GAIN_10;
    pv2Voltage = values[1] / GAIN_10;
    pv1Current = values[2] / GAIN_10;
    pv2Current = values[3] / GAIN_10;
    return true;
}

bool Inverter::getSystemStatus(float &temperature, int &exportPercent, int &outputPower)
{
    std::vector<uint16_t> values;
    if (!modbusHandler_.readRegisters(REG_TEMPERATURE, 3, values, SLAVE_ADDRESS))
        return false;
    temperature = values[0] / GAIN_10;
    exportPercent = static_cast<int>(values[1] / GAIN_1);
    outputPower = static_cast<int>(values[2] / GAIN_1);
    return true;
}

// Write operations
bool Inverter::setExportPowerPercent(int value)
{
    // Clamp the value to valid range (0-100)
    int clampedValue = value;
    if (value < 0)
        clampedValue = 0;
    else if (value > 100)
        clampedValue = 100;

    // Warn if the value was clamped
    if (clampedValue != value)
    {
        std::cerr << "Warning: Export power percentage " << value
                  << " is out of range. Clamped to " << clampedValue << std::endl;
    }

    return modbusHandler_.writeRegister(REG_EXPORT_POWER_PERCENT, static_cast<uint16_t>(clampedValue * GAIN_1), SLAVE_ADDRESS);
}

ModbusHandler &Inverter::getModbusHandler()
{
    return modbusHandler_;
}
