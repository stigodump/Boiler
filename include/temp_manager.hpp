#pragma once

#include "sensor_config.hpp"
#include <cstdint>

namespace boiler {

struct TemperatureSensor {
  const SensorConfig *sensor_id = nullptr;
  float temp_c = 0.0f;
  bool is_valid = false;
  uint8_t fail_count = 0;
};

/// Callback for when temperature changes.
using TempChangeCallback = void (*)(const TemperatureSensor *sensor);

/// Callback for when a sensor fails X times.
using TempFailureCallback = void (*)(const TemperatureSensor *sensor);

/**
 * @brief Manages multiple DS18B20 temperature sensors with interleaved polling.
 *
 * Polling occurs every 500ms, advancing to the next registered sensor.
 */
class TempManager {
public:
  static constexpr int MAX_SENSORS = 10;

  TempManager();

  /**
   * @brief Initializes all registered sensors by starting their first
   * conversion.
   */
  void init();

  /**
   * @brief Add a sensor to the manager state machine.
   * @param rom ROM ID of the sensor (8 bytes).
   * @return true if added, false if maximum sensors reached.
   */
  bool add_sensor(const uint8_t *rom);

  /**
   * @brief Update the state machine. Should be called frequently in the main
   * loop.
   */
  void update();

  /**
   * @brief Set the callback for temperature change events.
   */
  void set_on_change(TempChangeCallback cb) { on_change_cb_ = cb; }

  /**
   * @brief Set the callback for sensor failure events.
   * @param threshold Number of consecutive failures before triggering callback.
   */
  void set_on_failure(TempFailureCallback cb, int threshold = 5) {
    on_failure_cb_ = cb;
    failure_threshold_ = threshold;
  }

  /**
   * @brief Get the last read temperature for a sensor by its friendly name in
   * the config.
   * @return A pointer to the TemperatureSensor or nullptr if not found.
   */
  const TemperatureSensor *get_sensor_by_name(const char *name) const;

private:
  TemperatureSensor sensors_[MAX_SENSORS];
  int num_sensors_ = 0;

  int failure_threshold_ = 5;

  uint32_t last_update_ms_ = 0;
  int current_sensor_idx_ = 0; // Current index in sensors_ array

  TempChangeCallback on_change_cb_ = nullptr;
  TempFailureCallback on_failure_cb_ = nullptr;

  void poll_sensor(int idx);
};

} // namespace boiler
