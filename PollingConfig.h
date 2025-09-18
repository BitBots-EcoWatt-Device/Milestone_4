#ifndef POLLING_CONFIG_H
#define POLLING_CONFIG_H

#include <map>
#include <set>
#include <string>
#include <functional>
#include <vector>

// Forward declaration
class Inverter;

// ================= Polling Configuration ==================
enum class ParameterType
{
    AC_VOLTAGE,
    AC_CURRENT,
    AC_FREQUENCY,
    PV1_VOLTAGE,
    PV2_VOLTAGE,
    PV1_CURRENT,
    PV2_CURRENT,
    TEMPERATURE,
    EXPORT_POWER_PERCENT,
    OUTPUT_POWER
};

struct ParameterConfig
{
    ParameterType type;
    std::string name;
    std::string unit;
    std::function<bool(Inverter &, float &)> readFunction;

    ParameterConfig() = default;

    ParameterConfig(ParameterType t, const std::string &n, const std::string &u,
                    std::function<bool(Inverter &, float &)> func)
        : type(t), name(n), unit(u), readFunction(func) {}
};

class PollingConfig
{
public:
    PollingConfig();

    void addParameter(ParameterType param);
    void removeParameter(ParameterType param);
    void setParameters(const std::vector<ParameterType> &params);

    const std::set<ParameterType> &getEnabledParameters() const;
    const ParameterConfig &getParameterConfig(ParameterType param) const;

    void printEnabledParameters() const;

    // Predefined monitoring profiles for common use cases
    void setBasicACProfile();
    void setComprehensiveProfile();
    void setPVMonitoringProfile();
    void setThermalProfile();

private:
    std::map<ParameterType, ParameterConfig> availableParams_;
    std::set<ParameterType> enabledParams_;

    void initializeParameterConfigs();
};

// ================= Sample Structure ==================
struct Sample
{
    std::map<ParameterType, float> values;
    long long timestamp;

    Sample() = default;

    void setValue(ParameterType param, float value);
    float getValue(ParameterType param) const;
    bool hasValue(ParameterType param) const;
};

#endif
