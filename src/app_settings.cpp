#include "app_settings.hpp"
#include "boiler_board.hpp"
#include "common/logger.hpp"
#include "eeprom_layout.hpp"
#include "eeprom_manager.hpp"

extern common::Logger<boiler_board::Console> log_core0;

namespace boiler {

static constexpr uint32_t kSettingsMagic = 0xDEADBEEF;

AppSettings::AppSettings()
    : boiler_temp_(40), burner_runtime_sec_(0),
      pump_run_time_sec_(180) { // Default reasonable temp and 3min pump overrun
}

void AppSettings::load() {
  AppSettingsData data;

  if (eeprom_manager::load_app_settings(data)) {
    if (data.boiler_temp >= kMinBoilerTemp &&
        data.boiler_temp <= kMaxBoilerTemp) {
      boiler_temp_ = data.boiler_temp;
      burner_runtime_sec_ = data.burner_runtime_sec;
      pump_run_time_sec_ = data.pump_run_time_sec;
      log_core0.info("[SETTING] Loaded boiler temp from EEPROM: %d C",
                     boiler_temp_);
      log_core0.info("[SETTING] Loaded burner runtime: %lu s",
                     burner_runtime_sec_);
      log_core0.info("[SETTING] Loaded pump run time: %lu s",
                     pump_run_time_sec_);
    } else {
      log_core0.warn(
          "[SETTING] EEPROM target temp out of bounds, using default.");
    }
  } else {
    log_core0.info("[SETTING] Using default AppSettings.");
  }
}

void AppSettings::save() {
  AppSettingsData data;
  data.boiler_temp = boiler_temp_;
  data.burner_runtime_sec = burner_runtime_sec_;
  data.pump_run_time_sec = pump_run_time_sec_;

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

void AppSettings::add_burner_runtime(uint32_t seconds) {
  if (seconds > 0) {
    burner_runtime_sec_ += seconds;
    save();
  }
}

bool AppSettings::set_pump_run_time_sec(uint32_t sec) {
  if (sec <= kMaxPumpRunTime) {
    if (pump_run_time_sec_ != sec) {
      pump_run_time_sec_ = sec;
      save();
    }
    return true;
  }
  return false;
}

} // namespace boiler
