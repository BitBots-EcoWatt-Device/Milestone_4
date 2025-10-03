#ifndef ESP8266_INVERTER_H
#define ESP8266_INVERTER_H

#include <Arduino.h>
#include "ESP8266ModbusHandler.h"
#include "ESP8266DataTypes.h"
#include <vector>

class ESP8266Inverter
{
public:
    ESP8266Inverter();

    bool begin();

    // Set the Modbus slave address from configuration
    void setSlaveAddress(uint8_t slaveAddr);

    // New unified read method using parameter descriptor table
    bool read(ParameterType id, float &out);

    // New unified read method using parameter descriptor table
    bool read(ParameterType id, float &out);

    // Individual register read operations (legacy, now thin wrappers)
    bool getACVoltage(float &voltage);              // Register 0: Vac1/L1 Phase voltage
    bool getACCurrent(float &current);              // Register 1: Iac1/L1 Phase current
    bool getACFrequency(float &frequency);          // Register 2: Fac1/L1 Phase frequency
    bool getPV1Voltage(float &voltage);             // Register 3: Vpv1/PV1 input voltage
    bool getPV2Voltage(float &voltage);             // Register 4: Vpv2/PV2 input voltage
    bool getPV1Current(float &current);             // Register 5: Ipv1/PV1 input current
    bool getPV2Current(float &current);             // Register 6: Ipv2/PV2 input current
    bool getTemperature(float &temperature);        // Register 7: Inverter internal temperature
    bool getExportPowerPercent(int &exportPercent); // Register 8: Export power percentage
    bool getOutputPower(int &power);                // Register 9: Inverter current output power

    // Combined read operations for efficiency
    bool getACMeasurements(float &voltage, float &current, float &frequency);
    bool getPVMeasurements(float &pv1Voltage, float &pv2Voltage, float &pv1Current, float &pv2Current);
    bool getSystemStatus(float &temperature, int &exportPercent, int &outputPower);

    // Write operations
    bool setExportPowerPercent(int value); // Register 8: Set export power percentage

    // Direct access to Modbus operations if needed
    ESP8266ModbusHandler &getModbusHandler() { return modbusHandler_; }

private:
    ESP8266ModbusHandler modbusHandler_;
    uint8_t slaveAddress_;

    // Device-specific constants - Register addresses
    static const uint16_t REG_AC_VOLTAGE = 0;           // Vac1/L1 Phase voltage (gain: 10, unit: V)
    static const uint16_t REG_AC_CURRENT = 1;           // Iac1/L1 Phase current (gain: 10, unit: A)
    static const uint16_t REG_AC_FREQUENCY = 2;         // Fac1/L1 Phase frequency (gain: 100, unit: Hz)
    static const uint16_t REG_PV1_VOLTAGE = 3;          // Vpv1/PV1 input voltage (gain: 10, unit: V)
    static const uint16_t REG_PV2_VOLTAGE = 4;          // Vpv2/PV2 input voltage (gain: 10, unit: V)
    static const uint16_t REG_PV1_CURRENT = 5;          // Ipv1/PV1 input current (gain: 10, unit: A)
    static const uint16_t REG_PV2_CURRENT = 6;          // Ipv2/PV2 input current (gain: 10, unit: A)
    static const uint16_t REG_TEMPERATURE = 7;          // Inverter internal temperature (gain: 10, unit: °C)
    static const uint16_t REG_EXPORT_POWER_PERCENT = 8; // Export power percentage (gain: 1, unit: %)
    static const uint16_t REG_OUTPUT_POWER = 9;         // Inverter current output power (gain: 1, unit: W)

    // Helper functions
    bool readSingleRegister(uint16_t regAddr, uint16_t &value);
    bool writeSingleRegister(uint16_t regAddr, uint16_t value);
};

#endif // ESP8266_INVERTER_H