#pragma once

#include <cstdint>

namespace boiler {

/// @brief Controls the Burner and Pump based on temperature setpoints and current readings.
class BurnerControl {
public:
    using OutputSetter = void(*)(bool);

    BurnerControl(OutputSetter burner_pin, OutputSetter pump_pin);

    void init();
    
    /// @brief Enable or disable automatic mode. When false, both outputs are forced off.
    void set_auto_mode(bool active);
    
    /// @brief Check if automatic mode is active.
    bool get_auto_mode() const;

    /// @brief Update the control loop. Should be called periodically.
    /// @param current_temp Current temperature from the primary boiler sensor.
    /// @param target_temp The user-configured target temperature.
    /// @param is_valid True if the temperature sensor is currently providing valid readings.
    /// @param current_time_ms Current system time in milliseconds.
    void update(float current_temp, int target_temp, bool is_valid, uint32_t current_time_ms);

    bool is_burner_on() const { return burner_on_; }
    bool is_pump_on() const { return pump_on_; }

private:
    OutputSetter set_burner_;
    OutputSetter set_pump_;

    bool auto_mode_ = false;
    bool burner_on_ = false;
    bool pump_on_ = false;
    
    // Hysteresis parameters
    float low_hysteresis_ = 2.0f; // Turn on when temp <= target - 2.0
    float high_hysteresis_ = 0.0f; // Turn off when temp >= target + 0.0

    // Anti-short-cycle settings
    uint32_t last_burner_off_time_ = 0;
    static constexpr uint32_t kMinOffTimeMs = 60000; // 1 minute minimum off time
};

} // namespace boiler
