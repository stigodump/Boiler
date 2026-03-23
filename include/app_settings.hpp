#pragma once

#include <cstdint>

namespace boiler {

/// @brief Represents persistent application settings and user configurations.
class AppSettings {
public:
    AppSettings();

    /// @brief Loads settings from EEPROM.
    void load();

    /// @brief Saves current settings to EEPROM.
    void save();

    // -- Boiler Temperature Configuration --

    /// @brief Gets the configured target boiler temperature in degrees Celsius.
    int get_boiler_temp() const { return boiler_temp_; }

    /// @brief Sets the configured target boiler temperature.
    /// @param temp Target temperature in degrees Celsius.
    /// @return True if the temperature was within valid bounds and updated.
    bool set_boiler_temp(int temp);

    // Optional bounds constants
    static constexpr int kMinBoilerTemp = 10;
    static constexpr int kMaxBoilerTemp = 80;

private:
    int boiler_temp_;
};

} // namespace boiler
