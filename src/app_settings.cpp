#include "app_settings.hpp"
#include "boiler_board.hpp"
#include "common/logger.hpp"
#include "eeprom_layout.hpp"
#include "eeprom_manager.hpp"

extern common::Logger<boiler_board::Console> log_core0;

namespace boiler {

static constexpr uint32_t kSettingsMagic = 0xDEADBEEF;

AppSettings::AppSettings() : boiler_temp_(40) { // Default reasonable temp
}

void AppSettings::load() {
  AppSettingsData data;

  if (eeprom_manager::load_app_settings(data)) {
    if (data.boiler_temp >= kMinBoilerTemp &&
        data.boiler_temp <= kMaxBoilerTemp) {
      boiler_temp_ = data.boiler_temp;
      log_core0.printf("Loaded boiler temp from EEPROM: %d C\r\n",
                       boiler_temp_);
    } else {
      log_core0.warn("EEPROM target temp out of bounds, using default.");
    }
  } else {
    log_core0.info("Using default AppSettings.");
  }
}

void AppSettings::save() {
  AppSettingsData data;
  data.boiler_temp = boiler_temp_;

  eeprom_manager::save_app_settings(data);
}

bool AppSettings::set_boiler_temp(int temp) {
  if (temp >= kMinBoilerTemp && temp <= kMaxBoilerTemp) {
    if (boiler_temp_ != temp) {
      boiler_temp_ = temp;
      save();
    }
    return true; // We could later notify observers or save to flash here
  }
  return false;
}

} // namespace boiler
