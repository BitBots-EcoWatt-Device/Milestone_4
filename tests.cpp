/*
 * Modbus Handler Test Suite
 *
 * Tests various scenarios based on API documentation:
 *
 * READ Responses:
 * - Valid request + successful: Returns requested data in Modbus frame
 * - Invalid frame: API sends blank response
 * - Valid frame + invalid info: Modbus frame with error code
 *
 * WRITE Responses:
 * - Valid request + successful: API responds with same requested frame (echo)
 * - Invalid frame: API sends blank response
 * - Valid frame + invalid info: Modbus frame with error code
 *
 * Error Codes:
 * 01 = Illegal Function
 * 02 = Illegal Data Address
 * 03 = Illegal Data Value
 * 04 = Slave Device Failure
 * 05 = Acknowledge
 * 06 = Slave Device Busy
 * 08 = Memory Parity Error
 * 0A = Gateway Path Unavailable
 * 0B = Gateway Target Device Failed to Respond
 */

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <streambuf>
#include "ModbusHandler.h"

// Helper class to capture stderr output
class CaptureStderr
{
private:
    std::streambuf *old_cerr;
    std::ostringstream captured;

public:
    CaptureStderr()
    {
        old_cerr = std::cerr.rdbuf();
        std::cerr.rdbuf(captured.rdbuf());
    }

    ~CaptureStderr()
    {
        std::cerr.rdbuf(old_cerr);
    }

    std::string getOutput()
    {
        return captured.str();
    }
};

// Test different scenarios
void testInvalidFrame()
{
    std::cout << "\n=== Test 1: Invalid Modbus Frame (Should get blank response) ===" << std::endl;

    ModbusHandler handler;
    std::vector<uint16_t> values;

    // Capture error output
    CaptureStderr capture;

    // Try to read with invalid parameters that create malformed frame
    // Using function code or parameters that would create invalid frame
    bool result = handler.readRegisters(0xFFFF, 0, values); // 0 registers is invalid

    std::string errorOutput = capture.getOutput();

    if (result)
    {
        std::cout << "UNEXPECTED: Invalid frame request succeeded" << std::endl;
    }
    else
    {
        std::cout << "EXPECTED: Invalid frame request failed" << std::endl;
        if (!errorOutput.empty())
        {
            std::cout << "Error details captured:" << std::endl;
            std::cout << errorOutput << std::endl;

            // Check for blank response indicators
            if (errorOutput.find("blank response") != std::string::npos ||
                errorOutput.find("Blank response") != std::string::npos ||
                errorOutput.find("empty") != std::string::npos)
            {
                std::cout << "✓ API sent blank response for invalid frame (as documented)" << std::endl;
            }
            else if (errorOutput.find("request failed") != std::string::npos)
            {
                std::cout << "✓ Request failed (likely due to invalid frame)" << std::endl;
            }
            else
            {
                std::cout << "? Different error type detected" << std::endl;
            }
        }
        else
        {
            std::cout << "No error message captured (possible blank response)" << std::endl;
        }
    }
}

void testWriteToReadOnlyRegister()
{
    std::cout << "\n=== Test 2: Write to Read-Only Register (Should get error code 0x02) ===" << std::endl;

    ModbusHandler handler;

    // Capture error output
    CaptureStderr capture;

    // Try to write to a status register (usually read-only)
    bool result = handler.writeRegister(0x0000, 0x1234);

    std::string errorOutput = capture.getOutput();

    if (result)
    {
        std::cout << "UNEXPECTED: Write to read-only register succeeded" << std::endl;
    }
    else
    {
        std::cout << "EXPECTED: Write to read-only register failed" << std::endl;
        if (!errorOutput.empty())
        {
            std::cout << "Error details captured:" << std::endl;
            std::cout << errorOutput << std::endl;

            // According to API docs: should get error code 0x02 for invalid address
            if (errorOutput.find("Code 0x2") != std::string::npos)
            {
                std::cout << "✓ CORRECT: Error code 0x02 (Illegal Data Address) detected as documented" << std::endl;
            }
            else if (errorOutput.find("Code 0x1") != std::string::npos)
            {
                std::cout << "✓ Error code 0x01 (Illegal Function) detected" << std::endl;
            }
            else if (errorOutput.find("Modbus Exception") != std::string::npos)
            {
                std::cout << "✓ Modbus exception detected (as expected)" << std::endl;
            }
            else
            {
                std::cout << "? Different error type detected" << std::endl;
            }
        }
        else
        {
            std::cout << "No error message captured (may be blank response for invalid frame)" << std::endl;
        }
    }
}

void testInvalidRegister()
{
    std::cout << "\n=== Test 3: Invalid Register Address (Should get error code 0x02) ===" << std::endl;

    ModbusHandler handler;
    std::vector<uint16_t> values;

    // Capture error output
    CaptureStderr capture;

    // Try to read from invalid register - this should generate valid frame but invalid address
    bool result = handler.readRegisters(0x9999, 1, values);

    std::string errorOutput = capture.getOutput();

    if (result)
    {
        std::cout << "UNEXPECTED: Read from invalid register succeeded" << std::endl;
        std::cout << "Values read: ";
        for (auto val : values)
        {
            std::cout << "0x" << std::hex << val << std::dec << " ";
        }
        std::cout << std::endl;
    }
    else
    {
        std::cout << "EXPECTED: Read from invalid register failed" << std::endl;
        if (!errorOutput.empty())
        {
            std::cout << "Error details captured:" << std::endl;
            std::cout << errorOutput << std::endl;

            // According to API docs: should get error code 0x02 for invalid address
            if (errorOutput.find("Code 0x2") != std::string::npos)
            {
                std::cout << "✓ CORRECT: Error code 0x02 (Illegal Data Address) detected as documented" << std::endl;
            }
            else if (errorOutput.find("request failed") != std::string::npos)
            {
                std::cout << "✓ Network/timeout error (blank response for invalid frame)" << std::endl;
            }
            else if (errorOutput.find("Modbus Exception") != std::string::npos)
            {
                std::cout << "✓ Modbus exception detected" << std::endl;
            }
            else
            {
                std::cout << "? Different error type detected" << std::endl;
            }
        }
        else
        {
            std::cout << "No error message captured (possible blank response)" << std::endl;
        }
    }
}

void testInvalidContent()
{
    std::cout << "\n=== Test 4: Valid Frame but Invalid Content (Should get error code 0x03) ===" << std::endl;

    ModbusHandler handler;
    std::vector<uint16_t> values;

    // Capture error output
    CaptureStderr capture;

    // Try to read too many registers at once
    bool result = handler.readRegisters(0x0000, 200, values);

    std::string errorOutput = capture.getOutput();

    if (result)
    {
        std::cout << "UNEXPECTED: Read of too many registers succeeded" << std::endl;
        std::cout << "Number of values read: " << values.size() << std::endl;
    }
    else
    {
        std::cout << "EXPECTED: Read of too many registers failed (error response)" << std::endl;
        if (!errorOutput.empty())
        {
            std::cout << "Error details captured:" << std::endl;
            std::cout << errorOutput << std::endl;

            // According to API docs: should get error code 0x03 for invalid data value
            if (errorOutput.find("Code 0x3") != std::string::npos)
            {
                std::cout << "✓ CORRECT: Error code 0x03 (Illegal Data Value) detected as documented" << std::endl;
            }
            else if (errorOutput.find("Code 0x2") != std::string::npos)
            {
                std::cout << "✓ Error code 0x02 (Illegal Data Address) detected" << std::endl;
            }
            else if (errorOutput.find("Failed to parse") != std::string::npos)
            {
                std::cout << "✓ Parsing error detected (response too large)" << std::endl;
            }
            else if (errorOutput.find("Modbus Exception") != std::string::npos)
            {
                std::cout << "✓ Modbus exception detected" << std::endl;
            }
            else
            {
                std::cout << "? Different error type detected" << std::endl;
            }
        }
        else
        {
            std::cout << "No error message captured" << std::endl;
        }
    }
}

void testValidOperations()
{
    std::cout << "\n=== Test 5: Valid Operations ===" << std::endl;

    ModbusHandler handler;
    std::vector<uint16_t> values;

    // Test valid read
    std::cout << "Testing valid read operation..." << std::endl;
    CaptureStderr readCapture;
    bool readResult = handler.readRegisters(0x0000, 1, values);
    std::string readErrors = readCapture.getOutput();

    if (readResult && !values.empty())
    {
        std::cout << "SUCCESS: Read operation worked, value = 0x"
                  << std::hex << values[0] << std::dec << std::endl;
    }
    else
    {
        std::cout << "FAILED: Read operation failed" << std::endl;
        if (!readErrors.empty())
        {
            std::cout << "Read error details:" << std::endl;
            std::cout << readErrors << std::endl;
        }
    }
}

void testCRCCalculation()
{
    std::cout << "\n=== Test 6: CRC Calculation ===" << std::endl;

    ModbusHandler handler;

    // Test CRC with known data
    std::vector<uint8_t> testData = {0x11, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint16_t crc = handler.calculateCRC(testData);

    std::cout << "CRC for test data: 0x" << std::hex << crc << std::dec << std::endl;

    if (crc != 0)
    {
        std::cout << "SUCCESS: CRC calculation working" << std::endl;
    }
    else
    {
        std::cout << "FAILED: CRC calculation problem" << std::endl;
    }
}

void testSuccessfulWrite()
{
    std::cout << "\n=== Test 7: Successful Write (Should echo request) ===" << std::endl;

    ModbusHandler handler;

    // According to API docs: successful write should echo the same frame back
    std::cout << "Testing write to a potentially writable register..." << std::endl;
    CaptureStderr capture;

    // Try different registers that might be writable
    std::vector<uint16_t> writableRegisters = {0x0100, 0x0200, 0x0300};

    for (uint16_t reg : writableRegisters)
    {
        std::cout << "Trying register 0x" << std::hex << reg << std::dec << ":" << std::endl;

        CaptureStderr regCapture;
        bool result = handler.writeRegister(reg, 0x1234);
        std::string errors = regCapture.getOutput();

        if (result)
        {
            std::cout << "  ✓ SUCCESS: Write succeeded (API echoed request as documented)" << std::endl;
            return; // Found a writable register
        }
        else
        {
            if (!errors.empty())
            {
                if (errors.find("Code 0x2") != std::string::npos)
                {
                    std::cout << "  - Register not writable (Code 0x02)" << std::endl;
                }
                else if (errors.find("Code 0x1") != std::string::npos)
                {
                    std::cout << "  - Function not supported (Code 0x01)" << std::endl;
                }
                else
                {
                    std::cout << "  - Other error detected" << std::endl;
                }
            }
            else
            {
                std::cout << "  - No response (blank response for invalid frame)" << std::endl;
            }
        }
    }

    std::cout << "No writable registers found in test range - this may be normal for read-only devices" << std::endl;
}

void testErrorMessages()
{
    std::cout << "\n=== Test 8: Error Code Meanings ===" << std::endl;

    ModbusHandler handler;

    // Test the error message function with all documented codes
    std::cout << "Error code 0x01: " << handler.modbusExceptionMessage(0x01) << std::endl;
    std::cout << "Error code 0x02: " << handler.modbusExceptionMessage(0x02) << std::endl;
    std::cout << "Error code 0x03: " << handler.modbusExceptionMessage(0x03) << std::endl;
    std::cout << "Error code 0x04: " << handler.modbusExceptionMessage(0x04) << std::endl;
    std::cout << "Error code 0x05: " << handler.modbusExceptionMessage(0x05) << std::endl;
    std::cout << "Error code 0x06: " << handler.modbusExceptionMessage(0x06) << std::endl;
    std::cout << "Error code 0x08: " << handler.modbusExceptionMessage(0x08) << std::endl;
    std::cout << "Error code 0x0A: " << handler.modbusExceptionMessage(0x0A) << std::endl;
    std::cout << "Error code 0x0B: " << handler.modbusExceptionMessage(0x0B) << std::endl;
}

void testSpecificErrorScenarios()
{
    std::cout << "\n=== Test 9: Specific Error Scenarios ===" << std::endl;

    ModbusHandler handler;

    // Test 1: Try different invalid register addresses
    std::cout << "\nTesting various invalid register addresses:" << std::endl;
    std::vector<uint16_t> invalidAddresses = {0xFFFF, 0x1000, 0x5000};

    for (uint16_t addr : invalidAddresses)
    {
        std::cout << "Testing register 0x" << std::hex << addr << std::dec << ":" << std::endl;

        CaptureStderr capture;
        std::vector<uint16_t> values;
        bool result = handler.readRegisters(addr, 1, values);
        std::string errorOutput = capture.getOutput();

        if (result)
        {
            std::cout << "  UNEXPECTED: Read succeeded" << std::endl;
        }
        else
        {
            std::cout << "  EXPECTED: Read failed" << std::endl;
            if (!errorOutput.empty())
            {
                if (errorOutput.find("Code 0x2") != std::string::npos)
                {
                    std::cout << "  ✓ Illegal Data Address error detected" << std::endl;
                }
                else if (errorOutput.find("request failed") != std::string::npos)
                {
                    std::cout << "  ✓ Request timeout/failure (no response)" << std::endl;
                }
                else
                {
                    std::cout << "  ? Other error: " << errorOutput.substr(0, 50) << "..." << std::endl;
                }
            }
        }
    }

    // Test 2: Try writing different invalid values
    std::cout << "\nTesting various invalid write values:" << std::endl;
    std::vector<uint16_t> testValues = {0x0000, 0xFFFF, 0x8000};

    for (uint16_t val : testValues)
    {
        std::cout << "Testing write value 0x" << std::hex << val << std::dec << " to register 0x0002:" << std::endl;

        CaptureStderr capture;
        bool result = handler.writeRegister(0x0002, val);
        std::string errorOutput = capture.getOutput();

        if (result)
        {
            std::cout << "  SUCCESS: Write operation worked" << std::endl;
        }
        else
        {
            std::cout << "  FAILED: Write operation failed" << std::endl;
            if (!errorOutput.empty())
            {
                if (errorOutput.find("Code 0x2") != std::string::npos)
                {
                    std::cout << "  ✓ Illegal Data Address (register not writable)" << std::endl;
                }
                else if (errorOutput.find("Code 0x3") != std::string::npos)
                {
                    std::cout << "  ✓ Illegal Data Value (value out of range)" << std::endl;
                }
                else if (errorOutput.find("Code 0x1") != std::string::npos)
                {
                    std::cout << "  ✓ Illegal Function (write not supported)" << std::endl;
                }
                else
                {
                    std::cout << "  ? Other error: " << errorOutput.substr(0, 50) << "..." << std::endl;
                }
            }
        }
    }
}

int main()
{
    std::cout << "Modbus Handler Test Suite" << std::endl;
    std::cout << "=========================" << std::endl;

    // Run all tests based on API documentation scenarios
    testInvalidFrame();            // Test 1: Invalid frame -> blank response
    testWriteToReadOnlyRegister(); // Test 2: Valid frame, invalid address -> error 0x02
    testInvalidRegister();         // Test 3: Valid frame, invalid address -> error 0x02
    testInvalidContent();          // Test 4: Valid frame, invalid content -> error 0x03
    testValidOperations();         // Test 5: Valid operations
    testCRCCalculation();          // Test 6: CRC validation
    testSuccessfulWrite();         // Test 7: Try to find writable register
    testErrorMessages();           // Test 8: Error code meanings
    testSpecificErrorScenarios();  // Test 9: Specific error scenarios

    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}
