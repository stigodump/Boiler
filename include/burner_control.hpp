#pragma once

#include <cstdint>

namespace boiler {

/// @brief Controls the Burner and Pump based on temperature setpoints and current readings.
class BurnerControl {
public:
    enum class State {
        Off,
        On,
        Failure
    };

    using OutputSetter = void(*)(bool);
    using StateChangeCallback = void(*)(State new_state, uint32_t duration_sec);
    using TempChangeCallback = void(*)(float calculated_temp);

    BurnerControl(OutputSetter burner_pin, OutputSetter pump_pin);

    void init();
    
    void set_state_change_callback(StateChangeCallback cb);
    void set_temp_change_callback(TempChangeCallback cb);
    State get_state() const { return current_state_; }
    
    /// @brief Enable or disable automatic mode. When false, both outputs are forced off.
    void set_auto_mode(bool active);
    
    /// @brief Check if automatic mode is active.
    bool get_auto_mode() const;

    /// @brief Main execution loop to evaluate thermostatic targets.
    /// @param current_temp_0 Current boiler reading from sensor 0
    /// @param valid_0 True if sensor 0 is trusted
    /// @param current_temp_1 Current boiler reading from sensor 1
    /// @param valid_1 True if sensor 1 is trusted
    /// @param target_temp Setpoint
    /// @param current_time_ms System clock tracking
    /// @param pump_run_time_sec Configured post-purge duration
    void update(float current_temp_0, bool valid_0, float current_temp_1, bool valid_1, int target_temp, uint32_t current_time_ms, uint32_t pump_run_time_sec);

    bool is_burner_on() const { return burner_on_; }
    bool is_pump_on() const { return pump_on_; }
    
    float get_calculated_temp() const { return calculated_temp_; }
    bool is_degraded() const { return degraded_mode_; }

private:
    OutputSetter set_burner_;
    OutputSetter set_pump_;

    bool auto_mode_ = false;
    bool burner_on_ = false;
    bool pump_on_ = false;
    State current_state_ = State::Off;
    StateChangeCallback state_cb_ = nullptr;
    TempChangeCallback temp_cb_ = nullptr;
    uint32_t burner_start_time_ms_ = 0;
    
    // Pump overrun tracking
    bool pump_overrun_active_ = false;
    uint32_t auto_mode_end_time_ms_ = 0;
    
    void set_internal_state(State new_state, uint32_t current_time_ms);

    // Hysteresis parameters
    float low_hysteresis_ = 2.0f; // Turn on when temp <= target - 2.0
    float high_hysteresis_ = 0.0f; // Turn off when temp >= target + 0.0

    // Anti-short-cycle settings
    uint32_t last_burner_off_time_ = 0;
    static constexpr uint32_t kMinOffTimeMs = 60000; // 1 minute minimum off time

    // Dual sensor and RoC state
    float calculated_temp_ = -100.0f;
    bool degraded_mode_ = false;

    struct ProbeState {
        float last_temp = -100.0f;
        uint32_t last_time_ms = 0;
    };
    ProbeState probe0_;
    ProbeState probe1_;

    static constexpr float kMaxTempDiff = 5.0f; // Max difference between probes before fault
    static constexpr float kMaxRoc = 2.0f;      // Max Rate of Change: 2.0 °C / sec
};

} // namespace boiler
