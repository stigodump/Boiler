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

  // -- Burner Runtime --

  /// @brief Gets the total cumulative seconds the burner has run.
  uint32_t get_burner_runtime() const { return burner_runtime_sec_; }

  /// @brief Adds seconds to the cumulative burner runtime and saves.
  void add_burner_runtime(uint32_t seconds);

  // -- Pump Run Time --

  /// @brief Gets the pump overrun duration in seconds.
  uint32_t get_pump_run_time_sec() const { return pump_run_time_sec_; }

  /// @brief Sets the pump overrun duration and saves.
  bool set_pump_run_time_sec(uint32_t sec);

  // Optional bounds constants
  static constexpr int kMinBoilerTemp = 10;
  static constexpr int kMaxBoilerTemp = 80;
  static constexpr uint32_t kMaxPumpRunTime = 1800;

private:
  int boiler_temp_;
  uint32_t burner_runtime_sec_;
  uint32_t pump_run_time_sec_;
};

} // namespace boiler
