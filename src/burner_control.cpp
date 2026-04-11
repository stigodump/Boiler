#include "burner_control.hpp"

namespace boiler {

BurnerControl::BurnerControl(OutputSetter burner_pin, OutputSetter pump_pin) 
    : set_burner_(burner_pin), set_pump_(pump_pin) {}

void BurnerControl::init() {
    auto_mode_ = false;
    burner_on_ = false;
    pump_on_ = false;
    current_state_ = State::Off;
    set_burner_(false);
    set_pump_(false);
}

void BurnerControl::set_state_change_callback(StateChangeCallback cb) {
    state_cb_ = cb;
}

void BurnerControl::set_internal_state(State new_state, uint32_t current_time_ms) {
    if (current_state_ != new_state) {
        uint32_t duration_sec = 0;
        
        if (current_state_ == State::On) {
            duration_sec = (current_time_ms - burner_start_time_ms_) / 1000;
        } else if (new_state == State::On) {
            burner_start_time_ms_ = current_time_ms;
            duration_sec = 0;
        }

        current_state_ = new_state;
        if (state_cb_) {
            state_cb_(current_state_, duration_sec);
        }
    }
}

void BurnerControl::set_auto_mode(bool active) {
    if (auto_mode_ != active) {
        auto_mode_ = active;
        // The main update() loop will intercept the !auto_mode_ transition
        // to gracefully kill the burner, record State::Off duration, and 
        // commence the pump overrun timer.
        if (!auto_mode_) {
            pump_overrun_active_ = true;
        }
    }
}

bool BurnerControl::get_auto_mode() const {
    return auto_mode_;
}

void BurnerControl::update(float current_temp, int target_temp, bool is_valid, uint32_t current_time_ms, uint32_t pump_run_time_sec) {
    if (pump_overrun_active_) {
        auto_mode_end_time_ms_ = current_time_ms;
        pump_overrun_active_ = false;
    }

    // Fail-safe: if manual mode or sensor is invalid, shutdown immediately.
    if (!is_valid) {
        set_internal_state(State::Failure, current_time_ms);
    } else if (!auto_mode_) {
        set_internal_state(State::Off, current_time_ms);
    }

    if (!auto_mode_ || !is_valid) {
        if (burner_on_) {
            burner_on_ = false;
            set_burner_(false);
            last_burner_off_time_ = current_time_ms;
        }
        
        if (pump_on_) {
            // Wait for overrun, or force stop if invalid sensor!
            if (!is_valid || (current_time_ms - auto_mode_end_time_ms_) >= (pump_run_time_sec * 1000)) {
                pump_on_ = false;
                set_pump_(false);
            }
        }
        return;
    }

    float target_f = static_cast<float>(target_temp);
    
    // Constant pump circulation while heating schedule is active
    if (!pump_on_) {
        pump_on_ = true;
        set_pump_(true);
    }
    
    if (!burner_on_) {
        // Evaluate if we should turn ON
        if (current_temp <= (target_f - low_hysteresis_)) {
            // Check anti-short cycle protection
            if ((current_time_ms - last_burner_off_time_) >= kMinOffTimeMs || last_burner_off_time_ == 0) {
                burner_on_ = true;
                set_burner_(true);
                set_internal_state(State::On, current_time_ms);
            } else {
                set_internal_state(State::Off, current_time_ms);
            }
        } else {
            set_internal_state(State::Off, current_time_ms);
        }
    } else {
        // Evaluate if we should turn OFF
        if (current_temp >= (target_f + high_hysteresis_)) {
            burner_on_ = false;
            set_burner_(false);
            last_burner_off_time_ = current_time_ms;
            set_internal_state(State::Off, current_time_ms);
        } else {
            set_internal_state(State::On, current_time_ms);
        }
    }
}

} // namespace boiler
