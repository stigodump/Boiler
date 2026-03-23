#include "burner_control.hpp"

namespace boiler {

BurnerControl::BurnerControl(OutputSetter burner_pin, OutputSetter pump_pin) 
    : set_burner_(burner_pin), set_pump_(pump_pin) {}

void BurnerControl::init() {
    auto_mode_ = false;
    burner_on_ = false;
    pump_on_ = false;
    set_burner_(false);
    set_pump_(false);
}

void BurnerControl::set_auto_mode(bool active) {
    if (auto_mode_ != active) {
        auto_mode_ = active;
        // If we just turned off auto mode, ensure hardware is disabled immediately.
        if (!auto_mode_) {
            burner_on_ = false;
            pump_on_ = false;
            set_burner_(false);
            set_pump_(false);
        }
    }
}

bool BurnerControl::get_auto_mode() const {
    return auto_mode_;
}

void BurnerControl::update(float current_temp, int target_temp, bool is_valid, uint32_t current_time_ms) {
    // Fail-safe: if manual mode or sensor is invalid, shutdown immediately.
    if (!auto_mode_ || !is_valid) {
        if (burner_on_) {
            burner_on_ = false;
            set_burner_(false);
            last_burner_off_time_ = current_time_ms;
        }
        if (pump_on_) {
            pump_on_ = false;
            set_pump_(false);
        }
        return;
    }

    float target_f = static_cast<float>(target_temp);
    
    if (!burner_on_) {
        // Evaluate if we should turn ON
        if (current_temp <= (target_f - low_hysteresis_)) {
            // Check anti-short cycle protection
            if ((current_time_ms - last_burner_off_time_) >= kMinOffTimeMs || last_burner_off_time_ == 0) {
                burner_on_ = true;
                pump_on_ = true;
                set_burner_(true);
                set_pump_(true);
            }
        }
    } else {
        // Evaluate if we should turn OFF
        if (current_temp >= (target_f + high_hysteresis_)) {
            burner_on_ = false;
            pump_on_ = false;
            set_burner_(false);
            set_pump_(false);
            last_burner_off_time_ = current_time_ms;
        }
    }
}

} // namespace boiler
