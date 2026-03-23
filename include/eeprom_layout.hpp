#pragma once

#include "common/alarm_timer.hpp"
#include <cstdint>

namespace boiler {

// Data for AppSettings
struct AppSettingsData {
  uint32_t magic;
  int boiler_temp;
};

struct AlarmSettingsData {
  uint32_t magic;
  common::AlarmTimer::Schedule schedule;
};

// Define the exact layout of the EEPROM
// This prevents modules from overwriting each other's data
struct EepromLayout {
  AppSettingsData app_settings;
  AlarmSettingsData alarms;

  // As you add more modules that need EEPROM storage,
  // define their structs above and add them here!
  // Example:
  // NetworkSettingsData network;
  // AlarmSettingsData alarms;
};

} // namespace boiler
