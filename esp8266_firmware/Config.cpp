#include "Config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

Config &Config::getInstance()
{
    static Config instance;
    return instance;
}

bool Config::loadFromFile(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open config file: " << filename << std::endl;
        return false;
    }

    config_.clear();
    std::string line;
    std::string currentSection;

    while (std::getline(file, line))
    {
        parseLine(line, currentSection);
    }

    file.close();
    loaded_ = true;

    // Validate required keys are present
    if (getApiKey().empty() || getReadUrl().empty() || getWriteUrl().empty())
    {
        std::cerr << "Error: Missing required configuration values in " << filename << std::endl;
        loaded_ = false;
        return false;
    }

    return true;
}

std::string Config::getApiKey() const
{
    return getValue("API", "api_key");
}

std::string Config::getReadUrl() const
{
    return getValue("ENDPOINTS", "read_url");
}

std::string Config::getWriteUrl() const
{
    return getValue("ENDPOINTS", "write_url");
}

uint8_t Config::getDefaultSlaveAddress() const
{
    std::string value = getValue("DEVICE", "default_slave_address");
    if (value.empty())
    {
        return 0x11; // Default fallback
    }

    // Handle hex format (0x11 or 11)
    if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X")
    {
        return static_cast<uint8_t>(std::stoul(value, nullptr, 16));
    }
    else
    {
        return static_cast<uint8_t>(std::stoul(value, nullptr, 16));
    }
}

bool Config::isLoaded() const
{
    return loaded_;
}

std::string Config::trim(const std::string &str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string Config::getValue(const std::string &section, const std::string &key) const
{
    std::string fullKey = section + "." + key;
    auto it = config_.find(fullKey);
    if (it != config_.end())
    {
        return it->second;
    }
    return "";
}

void Config::parseLine(const std::string &line, std::string &currentSection)
{
    std::string trimmedLine = trim(line);

    // Skip empty lines and comments
    if (trimmedLine.empty() || trimmedLine[0] == '#')
        return;

    // Check for section header
    if (trimmedLine[0] == '[' && trimmedLine.back() == ']')
    {
        currentSection = trimmedLine.substr(1, trimmedLine.length() - 2);
        return;
    }

    // Parse key=value pairs
    size_t equalPos = trimmedLine.find('=');
    if (equalPos != std::string::npos && !currentSection.empty())
    {
        std::string key = trim(trimmedLine.substr(0, equalPos));
        std::string value = trim(trimmedLine.substr(equalPos + 1));

        std::string fullKey = currentSection + "." + key;
        config_[fullKey] = value;
    }
}
