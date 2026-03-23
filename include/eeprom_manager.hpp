#pragma once

#include "eeprom_layout.hpp"
#include <cstdint>

namespace boiler {
namespace eeprom_manager {

/// @brief Loads AppSettings out of the EEPROM layout.
/// @param out_data Returns the loaded data.
/// @return true if the magic number is valid and settings are loaded.
bool load_app_settings(AppSettingsData& out_data);

/// @brief Saves AppSettings into the EEPROM layout.
/// @param data The data to save.
/// @return true on success.
bool save_app_settings(const AppSettingsData& data);

/// @brief Loads Alarm Schedules out of the EEPROM layout.
/// @param out_data Returns the loaded data.
/// @return true if the magic number is valid and schedules are loaded.
bool load_alarm_schedule(AlarmSettingsData& out_data);

/// @brief Saves Alarm Schedules into the EEPROM layout.
/// @param data The data to save.
/// @return true on success.
bool save_alarm_schedule(const AlarmSettingsData& data);

} // namespace eeprom_manager
} // namespace boiler
