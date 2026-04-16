#include "burner_control.hpp"
#include <cmath>

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

void BurnerControl::set_temp_change_callback(TempChangeCallback cb) {
  temp_cb_ = cb;
}

void BurnerControl::set_internal_state(State new_state,
                                       uint32_t current_time_ms) {
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

bool BurnerControl::get_auto_mode() const { return auto_mode_; }

void BurnerControl::update(float current_temp_0, bool valid_0,
                           float current_temp_1, bool valid_1, int target_temp,
                           uint32_t current_time_ms,
                           uint32_t pump_run_time_sec) {
  if (pump_overrun_active_) {
    auto_mode_end_time_ms_ = current_time_ms;
    pump_overrun_active_ = false;
  }

  // --- Dual Sensor Plausibility and RoC Check ---
  bool p0_trusted = valid_0;
  bool p1_trusted = valid_1;

  // RoC for Probe 0
  if (p0_trusted) {
    if (probe0_.last_time_ms > 0 && probe0_.last_temp > -99.0f) {
      float dt_sec = (current_time_ms - probe0_.last_time_ms) / 1000.0f;
      if (dt_sec > 0.1f) { // Need some elapsed time to check RoC
        float roc = std::abs(current_temp_0 - probe0_.last_temp) / dt_sec;
        if (roc > kMaxRoc) {
          p0_trusted = false; // RoC exceeded, mark untrusted!
        }
      }
    }
  }
  if (p0_trusted) {
    probe0_.last_temp = current_temp_0;
    probe0_.last_time_ms = current_time_ms;
  }

  // RoC for Probe 1
  if (p1_trusted) {
    if (probe1_.last_time_ms > 0 && probe1_.last_temp > -99.0f) {
      float dt_sec = (current_time_ms - probe1_.last_time_ms) / 1000.0f;
      if (dt_sec > 0.1f) {
        float roc = std::abs(current_temp_1 - probe1_.last_temp) / dt_sec;
        if (roc > kMaxRoc) {
          p1_trusted = false; // RoC exceeded!
        }
      }
    }
  }
  if (p1_trusted) {
    probe1_.last_temp = current_temp_1;
    probe1_.last_time_ms = current_time_ms;
  }

  // Cross-check if both are still trusted
  if (p0_trusted && p1_trusted) {
    float diff = std::abs(current_temp_0 - current_temp_1);
    if (diff > kMaxTempDiff) {
      // They differ too much! We cannot know which is true without more history
      // tracking. For now, if we hit max diff, we mark both untrusted to
      // fail-safe.
      p0_trusted = false;
      p1_trusted = false;
    }
  }

  // Calculate final temperature value
  float new_calc_temp = calculated_temp_;
  bool has_valid_reading = false;

  if (p0_trusted && p1_trusted) {
    new_calc_temp = (current_temp_0 + current_temp_1) / 2.0f;
    degraded_mode_ = false;
    has_valid_reading = true;
  } else if (p0_trusted) {
    new_calc_temp = current_temp_0;
    degraded_mode_ = true;
    has_valid_reading = true;
  } else if (p1_trusted) {
    new_calc_temp = current_temp_1;
    degraded_mode_ = true;
    has_valid_reading = true;
  } else {
    degraded_mode_ = true; // or complete failure
    has_valid_reading = false;
  }

  // Trigger temp change callback if changed meaningfully
  if (has_valid_reading && std::abs(new_calc_temp - calculated_temp_) >= 0.1f) {
    calculated_temp_ = new_calc_temp;
    if (temp_cb_) {
      temp_cb_(calculated_temp_);
    }
  } else if (has_valid_reading && calculated_temp_ < -99.0f) {
    calculated_temp_ = new_calc_temp;
    if (temp_cb_) {
      temp_cb_(calculated_temp_);
    }
  }

  // Fail-safe: if manual mode or NO sensor is valid, shutdown immediately.
  if (!has_valid_reading) {
    set_internal_state(State::Failure, current_time_ms);
  } else if (!auto_mode_) {
    set_internal_state(State::Off, current_time_ms);
  }

  if (!auto_mode_ || !has_valid_reading) {
    if (burner_on_) {
      burner_on_ = false;
      set_burner_(false);
      last_burner_off_time_ = current_time_ms;
    }

    if (pump_on_) {
      // Wait for overrun, or force stop if invalid sensor!
      if (!has_valid_reading || (current_time_ms - auto_mode_end_time_ms_) >=
                                    (pump_run_time_sec * 1000)) {
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
    if (calculated_temp_ <= (target_f - low_hysteresis_)) {
      // Check anti-short cycle protection
      if ((current_time_ms - last_burner_off_time_) >= kMinOffTimeMs ||
          last_burner_off_time_ == 0) {
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
    if (calculated_temp_ >= (target_f + high_hysteresis_)) {
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
