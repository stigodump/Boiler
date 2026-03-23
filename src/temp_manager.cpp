#include "temp_manager.hpp"
#include "boiler_board.hpp"
#include <cstring>

namespace boiler {

TempManager::TempManager() {
  for (int i = 0; i < MAX_SENSORS; ++i) {
    sensors_[i].sensor_id = nullptr;
    sensors_[i].temp_c = 0.0f;
    sensors_[i].is_valid = false;
    sensors_[i].fail_count = 0;
  }
}

void TempManager::init() {
  for (int i = 0; i < num_sensors_; ++i) {
    if (sensors_[i].sensor_id) {
      boiler_board::TempSensor::start_conversion(sensors_[i].sensor_id->rom);
    }
  }
}

const TemperatureSensor* TempManager::get_sensor_by_name(const char *name) const {
  if (!name) return nullptr;
  for (int i = 0; i < num_sensors_; ++i) {
    const SensorConfig* cfg = sensors_[i].sensor_id;
    if (cfg && cfg->name && std::strncmp(cfg->name, name, 32) == 0) {
      return &sensors_[i];
    }
  }
  return nullptr;
}

bool TempManager::add_sensor(const uint8_t *rom) {
  if (num_sensors_ >= MAX_SENSORS || rom == nullptr) {
    return false;
  }
  const SensorConfig *cfg = find_sensor_config(rom);
  if (!cfg) return false;
  sensors_[num_sensors_].sensor_id = cfg;
  num_sensors_++;
  return true;
}

void TempManager::update() {
  uint32_t now = board::Timer::ticks_us() / 1000;

  if (num_sensors_ == 0) {
    return;
  }

  // Poll every 500ms
  if (now - last_update_ms_ < 500) {
    return;
  }
  last_update_ms_ = now;

  poll_sensor(current_sensor_idx_);

  // Interleave
  current_sensor_idx_ = (current_sensor_idx_ + 1) % num_sensors_;
}

void TempManager::poll_sensor(int idx) {
  bool success = false;
  float current_temp = 0.0f;
  TemperatureSensor &state = sensors_[idx];
  
  if (!state.sensor_id) return;

  // Read value from previous conversion attempt (if any)
  success = boiler_board::TempSensor::read_temperature(current_temp, state.sensor_id->rom);

  if (success) {
    state.fail_count = 0; // Reset failure counter on success
    state.is_valid = true;

    // Check for change
    if (current_temp != state.temp_c) {
      state.temp_c = current_temp;
      if (on_change_cb_) {
        on_change_cb_(&state);
      }
    }
  } else {
    state.fail_count++;

    if (state.fail_count >= failure_threshold_) {
      state.is_valid = false;
      if (on_failure_cb_) {
        on_failure_cb_(&state);
      }
    }
  }

  // Start NEXT conversion for THIS sensor immediately
  // Since we interleave, it will have enough time to complete before we read it
  // again.
  boiler_board::TempSensor::start_conversion(state.sensor_id->rom);
}

} // namespace boiler
