#ifndef MODBUS_HANDLER_H
#define MODBUS_HANDLER_H

#include <cstdint>
#include "ProtocolAdapter.h"
#include <vector>
#include <string>

class ModbusHandler
{
public:
    ModbusHandler();

    // Core Modbus protocol operations
    bool readRegisters(uint16_t startAddr, uint16_t numRegs, std::vector<uint16_t> &values, uint8_t slaveAddr = 0x11);
    bool writeRegister(uint16_t regAddr, uint16_t regValue, uint8_t slaveAddr = 0x11);

    // CRC and error code helpers
    uint16_t calculateCRC(const std::vector<uint8_t> &data);
    std::string modbusExceptionMessage(uint8_t code);

private:
    ProtocolAdapter adapter_;

    // Helper functions
    std::string buildReadFrame(uint8_t slaveAddr, uint16_t startAddr, uint16_t numRegs);
    std::string buildWriteFrame(uint8_t slaveAddr, uint16_t regAddr, uint16_t regValue);
};

#endif
