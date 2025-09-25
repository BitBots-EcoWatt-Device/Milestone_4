#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <cstdint>

class Config
{
public:
    // Get singleton instance
    static Config &getInstance();

    // Load configuration from file
    bool loadFromFile(const std::string &filename = "config.ini");

    // Get configuration values
    std::string getApiKey() const;
    std::string getReadUrl() const;
    std::string getWriteUrl() const;
    uint8_t getDefaultSlaveAddress() const;

    // Check if configuration is loaded
    bool isLoaded() const;

private:
    Config() = default;
    ~Config() = default;
    Config(const Config &) = delete;
    Config &operator=(const Config &) = delete;

    std::map<std::string, std::string> config_;
    bool loaded_ = false;

    // Helper functions
    std::string trim(const std::string &str);
    std::string getValue(const std::string &section, const std::string &key) const;
    void parseLine(const std::string &line, std::string &currentSection);
};

#endif
