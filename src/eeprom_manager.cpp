#include "eeprom_manager.hpp"
#include "boiler_board.hpp"
#include "common/logger.hpp"
#include <cstddef>

extern common::Logger<boiler_board::Console> log_core0;

namespace boiler {
namespace eeprom_manager {

static constexpr uint32_t kAppSettingsMagic = 0xDEADBEE4;
static constexpr uint32_t kAlarmSettingsMagic = 0xA1A8A001;

bool load_app_settings(AppSettingsData& out_data) {
    uint16_t address = offsetof(EepromLayout, app_settings);
    
    if (boiler_board::Eeprom::read(address, reinterpret_cast<uint8_t*>(&out_data), sizeof(AppSettingsData))) {
        if (out_data.magic == kAppSettingsMagic) {
            return true;
        } else {
            log_core0.printf("[EEPROM] AppSettings magic mismatch (expected %X, got %X). Using defaults.\r\n", 
                           kAppSettingsMagic, out_data.magic);
        }
    } else {
        log_core0.error("[EEPROM] Failed to read AppSettings.");
    }
    return false;
}

bool save_app_settings(const AppSettingsData& data) {
    AppSettingsData write_data = data;
    write_data.magic = kAppSettingsMagic;
    
    uint16_t address = offsetof(EepromLayout, app_settings);
    
    if (boiler_board::Eeprom::write(address, reinterpret_cast<const uint8_t*>(&write_data), sizeof(AppSettingsData))) {
        log_core0.info("[EEPROM] Saved AppSettings successfully.");
        return true;
    } else {
        log_core0.error("[EEPROM] Failed to save AppSettings.");
        return false;
    }
}

bool load_alarm_schedule(AlarmSettingsData& out_data) {
    uint16_t address = offsetof(EepromLayout, alarms);
    
    if (boiler_board::Eeprom::read(address, reinterpret_cast<uint8_t*>(&out_data), sizeof(AlarmSettingsData))) {
        if (out_data.magic == kAlarmSettingsMagic) {
            return true;
        } else {
            log_core0.info("[EEPROM] AlarmSettings magic mismatch. Using defaults.");
        }
    } else {
        log_core0.error("[EEPROM] Failed to read AlarmSettings.");
    }
    return false;
}

bool save_alarm_schedule(const AlarmSettingsData& data) {
    AlarmSettingsData write_data = data;
    write_data.magic = kAlarmSettingsMagic;
    
    uint16_t address = offsetof(EepromLayout, alarms);
    
    if (boiler_board::Eeprom::write(address, reinterpret_cast<const uint8_t*>(&write_data), sizeof(AlarmSettingsData))) {
        log_core0.info("[EEPROM] Saved AlarmSchedules successfully.");
        return true;
    } else {
        log_core0.error("[EEPROM] Failed to save AlarmSchedules.");
        return false;
    }
}

} // namespace eeprom_manager
} // namespace boiler
