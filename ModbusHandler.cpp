#include "ModbusHandler.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <algorithm>
#include <cctype>

ModbusHandler::ModbusHandler() : adapter_() {}

// ========== Modbus CRC-16 ===========
uint16_t ModbusHandler::calculateCRC(const std::vector<uint8_t> &data)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < data.size(); ++i)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// ========== Error Code Handling ==========
std::string ModbusHandler::modbusExceptionMessage(uint8_t code)
{
    switch (code)
    {
    case 0x01:
        return "Illegal Function (function not supported)";
    case 0x02:
        return "Illegal Data Address (address not valid)";
    case 0x03:
        return "Illegal Data Value (value out of range)";
    case 0x04:
        return "Slave Device Failure";
    case 0x05:
        return "Acknowledge (request accepted, processing delayed)";
    case 0x06:
        return "Slave Device Busy";
    case 0x08:
        return "Memory Parity Error";
    case 0x0A:
        return "Gateway Path Unavailable";
    case 0x0B:
        return "Gateway Target Device Failed to Respond";
    default:
        return "Unknown Modbus Exception";
    }
}

// Helper: Build Modbus Read Holding Registers frame
std::string ModbusHandler::buildReadFrame(uint8_t slaveAddr, uint16_t startAddr, uint16_t numRegs)
{
    std::vector<uint8_t> frame = {
        slaveAddr,
        0x03,
        static_cast<uint8_t>(startAddr >> 8),
        static_cast<uint8_t>(startAddr & 0xFF),
        static_cast<uint8_t>(numRegs >> 8),
        static_cast<uint8_t>(numRegs & 0xFF)};
    uint16_t crc = calculateCRC(frame);
    frame.push_back(crc & 0xFF);
    frame.push_back((crc >> 8) & 0xFF);
    std::ostringstream oss;
    for (auto b : frame)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}

// Helper: Build Modbus Write Single Register frame
std::string ModbusHandler::buildWriteFrame(uint8_t slaveAddr, uint16_t regAddr, uint16_t regValue)
{
    std::vector<uint8_t> frame = {
        slaveAddr,
        0x06, // Function code for Write Single Register
        static_cast<uint8_t>(regAddr >> 8),
        static_cast<uint8_t>(regAddr & 0xFF),
        static_cast<uint8_t>(regValue >> 8),
        static_cast<uint8_t>(regValue & 0xFF)};
    uint16_t crc = calculateCRC(frame);
    frame.push_back(crc & 0xFF);
    frame.push_back((crc >> 8) & 0xFF);
    std::ostringstream oss;
    for (auto b : frame)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}

// Helper: Parse Modbus Read response (returns vector of register values)
std::vector<uint16_t> parseReadResponse(const std::string &resp, int numRegs)
{
    std::vector<uint16_t> regs;
    if (resp.size() < 6 + static_cast<size_t>(numRegs) * 4)
        return regs;
    for (int i = 0; i < numRegs; ++i)
    {
        int idx = 6 + i * 4;
        uint16_t val = static_cast<uint16_t>(std::stoi(resp.substr(idx, 4), nullptr, 16));
        regs.push_back(val);
    }
    return regs;
}

// Dynamic register read with retry, CRC, error code handling
bool ModbusHandler::readRegisters(uint16_t startAddr, uint16_t numRegs, std::vector<uint16_t> &values, uint8_t slaveAddr)
{
    std::string resp;
    std::string req = buildReadFrame(slaveAddr, startAddr, numRegs);
    int attempts = 0;
    while (attempts < 3)
    {
        if (!adapter_.sendReadRequest(req, resp))
        {
            std::cerr << "Read request failed (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        if (resp.empty() || resp.size() < 8)
        {
            std::cerr << "Malformed or blank response (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        // CRC check
        std::vector<uint8_t> frameBytes;
        for (size_t i = 0; i < resp.size(); i += 2)
        {
            frameBytes.push_back(static_cast<uint8_t>(std::stoi(resp.substr(i, 2), nullptr, 16)));
        }
        if (frameBytes.size() < 4)
        {
            std::cerr << "Malformed frame (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        uint16_t receivedCRC = static_cast<uint16_t>(frameBytes[frameBytes.size() - 2] | (frameBytes[frameBytes.size() - 1] << 8));
        uint16_t calcCRC = calculateCRC(std::vector<uint8_t>(frameBytes.begin(), frameBytes.end() - 2));
        if (receivedCRC != calcCRC)
        {
            std::cerr << "CRC error: received " << std::hex << receivedCRC << ", calculated " << calcCRC << std::dec << " (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        // Modbus error code handling
        if (frameBytes.size() >= 5 && (frameBytes[1] & 0x80))
        {
            uint8_t excCode = frameBytes[2];
            std::cerr << "Modbus Exception: Code 0x" << std::hex << (int)excCode << ": " << modbusExceptionMessage(excCode) << std::dec << " (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        // Parse values
        values = parseReadResponse(resp, numRegs);
        if (!values.empty())
            return true;
        std::cerr << "Failed to parse register values (attempt " << (attempts + 1) << ")\n";
        attempts++;
    }
    return false;
}

// Write single register with retry, CRC, error code handling
bool ModbusHandler::writeRegister(uint16_t regAddr, uint16_t regValue, uint8_t slaveAddr)
{
    std::string resp;
    std::string req = buildWriteFrame(slaveAddr, regAddr, regValue);

    auto normalize_hex = [](std::string s)
    {
        s.erase(std::remove_if(s.begin(), s.end(),
                               [](unsigned char c)
                               { return std::isspace(c); }),
                s.end());
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::toupper(c)); });
        return s;
    };

    int attempts = 0;
    while (attempts < 3)
    {
        if (!adapter_.sendWriteRequest(req, resp))
        {
            std::cerr << "Write request failed (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        if (resp.empty())
        {
            std::cerr << "Blank response to write (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        // CRC check
        std::vector<uint8_t> frameBytes;
        for (size_t i = 0; i < resp.size(); i += 2)
        {
            frameBytes.push_back(static_cast<uint8_t>(std::stoi(resp.substr(i, 2), nullptr, 16)));
        }
        if (frameBytes.size() < 4)
        {
            std::cerr << "Malformed frame (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        uint16_t receivedCRC = static_cast<uint16_t>(frameBytes[frameBytes.size() - 2] | (frameBytes[frameBytes.size() - 1] << 8));
        uint16_t calcCRC = calculateCRC(std::vector<uint8_t>(frameBytes.begin(), frameBytes.end() - 2));
        if (receivedCRC != calcCRC)
        {
            std::cerr << "CRC error: received " << std::hex << receivedCRC << ", calculated " << calcCRC << std::dec << " (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        // Modbus error code handling
        if (frameBytes.size() >= 5 && (frameBytes[1] & 0x80))
        {
            uint8_t excCode = frameBytes[2];
            std::cerr << "Modbus Exception: Code 0x" << std::hex << (int)excCode << ": " << modbusExceptionMessage(excCode) << std::dec << " (attempt " << (attempts + 1) << ")\n";
            attempts++;
            continue;
        }
        if (normalize_hex(resp) == normalize_hex(req))
            return true;
        std::cerr << "Write response mismatch (attempt " << (attempts + 1) << ")\n";
        attempts++;
    }
    return false;
}
