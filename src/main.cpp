#include "common/alarm_timer.hpp"
#include "common/fonts/font10x16.hpp"
#include "common/logger.hpp"
#include "common/nmea_zda.hpp"

#include "app_settings.hpp"
#include "boiler_board.hpp"
#include "boiler_menu.hpp"
#include "burner_control.hpp"
#include "clock_manager.hpp"
#include "eeprom_manager.hpp"
#include "ipc_handler.hpp"
#include "menu_system.hpp"
#include "sensor_config.hpp"
#include "temp_manager.hpp"

using namespace boiler_board;

// Global loggers
common::Logger<Console> log_core0;
common::Logger<Console> log_core1;

// IPC Handler
static boiler::IpcHandler ipc;

// Temperature Manager
static boiler::TempManager temp_manager;

// Menu System
static boiler::MenuSystem menu_system;

// System Clock
using SysClock = ClockManager<boiler_board::DS1307Rtc, board::PicoRtc>;

// Global App Settings
boiler::AppSettings app_settings;

// Burner Control (Relay4 = Burner, Relay3 = Pump)
boiler::BurnerControl burner_control(boiler_board::BurnerRelay::set,
                                     boiler_board::PumpRelay::set);

// Alarm Timer (Global for menu access)
common::AlarmTimer alarm_timer;

// Control timer callbacks
void on_timer_on() {
  log_core0.info("[TIMER] Alarm Timer triggered ON event!");
  burner_control.set_auto_mode(true);
}

void on_timer_off() {
  log_core0.info("[TIMER] Alarm Timer triggered OFF event!");
  burner_control.set_auto_mode(false);
}

// MQTT Callback function for Core 0 logic
void on_mqtt_message(const char *topic, const uint8_t *payload, size_t len) {
  // Handle incoming commands or synchronization here
  if (strncmp(topic, "weather_station/gnss/telegram/ZDA", 33) == 0) {
    common::NmeaZda zda_time;
    if (common::parse_zda((const char *)payload, zda_time)) {
      // Feed GNSS time into our system clock manager
      datetime_t dt;
      dt.year = zda_time.year;
      dt.month = zda_time.month;
      dt.day = zda_time.day;
      dt.hour = zda_time.hour;
      dt.min = zda_time.minute;
      dt.sec = zda_time.second;
      dt.dotw = 0; // Automatically resolved internally via mktime
      SysClock::set_time(dt);

    } else {
      log_core0.warn("[TIME] Failed to parse ZDA string");
    }
  } else if (strncmp(topic, "home/boiler/set", 15) == 0) {
    // Process boiler set commands (e.g., target temp, manual override)
    log_core0.info("[MQTT] Received boiler set command");
  }
}

// Status callbacks
void on_link_up() { board::Led::set(true); }
void on_link_down() { board::Led::set(false); }
void on_network_up() {}
void on_network_down() {}
void on_mqtt_connected() {
  ipc.subscribe_topic("/weather_station/gnss/telegram/ZDA");
}
void on_mqtt_disconnected() {}

// Temp Manager callbacks
void on_temp_change(const boiler::TemperatureSensor *sensor) {
  if (!sensor || !sensor->sensor_id) {
    log_core0.warn("[TEMP] Received change callback for invalid sensor.");
    return;
  }

  // Publish telemetry
  if (ipc.get_status().is_mqtt_connected) {
    char payload[sizeof(network_core::ipc::MqttMessage::payload)];
    char topic[sizeof(network_core::ipc::MqttMessage::topic)];
    snprintf(topic, sizeof(topic), "temperature/%s", sensor->sensor_id->name);
    snprintf(payload, sizeof(payload), "%.2f", sensor->temp_c);
    ipc.publish_telemetry(topic, payload);
  }
}

void on_temp_failure(const boiler::TemperatureSensor *sensor) {
  if (!sensor || !sensor->sensor_id) {
    log_core0.warn("[TEMP] Received failure callback for invalid sensor.");
    return;
  }

  log_core0.printf("[ERR] [TEMP] %s FAIL (count: %d)\r\n",
                   sensor->sensor_id->name, sensor->fail_count);
}

void on_schedule_change(const common::AlarmTimer::Schedule &slots) {
  log_core0.info("[ALARM] Schedule changed");
  boiler::AlarmSettingsData data;
  for (int d = 0; d < 7; ++d) {
    for (int i = 0; i < common::AlarmTimer::kMaxSlotsPerDay; ++i) {
      data.schedule[d][i] = slots[d][i];
    }
  }
  boiler::eeprom_manager::save_alarm_schedule(data);
}

void display_home_screen() {
  const common::fonts::FontDef<uint16_t> &font = common::fonts::Font10x16_Def;
  const uint8_t scale = 1;
  const uint8_t char_width = (font.width + 1) * scale;
  const uint8_t char_height = (font.height + 2) * scale;

  datetime_t current_time = SysClock::get_current_time();
  int centered = 0;

  // --- Draw Time ---
  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", current_time.hour,
           current_time.min, current_time.sec);
  centered =
      (LcdHat::Display::ScreenWidth - (strlen(time_str) * char_width)) / 2;
  if (centered < 0)
    centered = 0;
  LcdHat::Display::draw_string(centered, 15, time_str, 0xFFFF, 0x0000, font,
                               scale);

  // --- Draw Date ---
  char date_str[16];
  snprintf(date_str, sizeof(date_str), "%02d-%02d-%04d", current_time.day,
           current_time.month, current_time.year);
  centered =
      (LcdHat::Display::ScreenWidth - (strlen(date_str) * char_width)) / 2;
  if (centered < 0)
    centered = 0;
  LcdHat::Display::draw_string(centered, 40, date_str, 0xFFFF, 0x0000, font,
                               scale);

  // Find the primary boiler temperature sensor by name
  float boiler_temp = -100.0f;
  bool boiler_found = false;

  const boiler::TemperatureSensor *boiler_sensor =
      temp_manager.get_sensor_by_name("boiler_core");
  if (boiler_sensor && boiler_sensor->is_valid) {
    boiler_temp = boiler_sensor->temp_c;
    boiler_found = true;
  }

  // Display Boiler Temp
  char temp_str[32];
  if (boiler_found) {
    snprintf(temp_str, sizeof(temp_str),
             "Boiler: %04.1f\x7F"
             "C",
             boiler_temp);
  } else {
    snprintf(temp_str, sizeof(temp_str), "Boiler: Wait/Err");
  }
  centered =
      (LcdHat::Display::ScreenWidth - (strlen(temp_str) * char_width)) / 2;
  if (centered < 0)
    centered = 0;
  LcdHat::Display::draw_string(centered, 80, temp_str, 0xFFFF, 0x0000, font,
                               scale);

  // Display Alarm Timer Status
  char timer_str[32];
  alarm_timer.get_transition_str(timer_str, sizeof(timer_str));
  char padded_timer[32];
  snprintf(padded_timer, sizeof(padded_timer), "%s", timer_str);
  centered =
      (LcdHat::Display::ScreenWidth - (strlen(padded_timer) * char_width)) / 2;
  if (centered < 0)
    centered = 0;
  LcdHat::Display::draw_string(centered, 110, padded_timer, 0x07E0, 0x0000,
                               font, scale);
}

int main() {
  // --- Init board peripherals ---
  board::Led::init();

  // --- Init RTC/EEPROM I2C ---
  boiler_board::RtcI2c::init();

  // --- Init System Clock ---
  SysClock::init();

  // --- Init Relays ---
  Relay1::init();
  Relay2::init();
  PumpRelay::init();
  BurnerRelay::init();

  // --- Console UART ---
  Console::set_pins(/*tx*/ 0, /*rx*/ 1);
  Console::open();

  // --- Initialize Temperature Sensor ---
  TempSensor::init();

  // --- Initialize Display & Joystick  ---
  LcdHat::init();
  LcdHat::Display::set_backlight(true);

  // --- Flash Programming Synchronization ---
  // Allow Core 1 (NetworkCore OTA) to pause Core 0 execution seamlessly 
  // during flash erase/program cycles to prevent fatal XIP bus deadlocks.
  // We handle this natively inside ipc_handler.cpp's ipc_ram_pause_loop now.
  // multicore_lockout_victim_init(); // REMOVED to prevent IPC message theft

  log_core0.info("Boiler System Starting on Core 0...");

  // --- Load Settings from EEPROM (must be after I2C + Console init) ---
  app_settings.load();

  // --- Initialize Alarm Timer ---
  alarm_timer.init();

  // --- Load Alarm Schedule from EEPROM ---
  boiler::AlarmSettingsData alarm_data;
  if (boiler::eeprom_manager::load_alarm_schedule(alarm_data)) {
    log_core0.info("Loaded alarm schedule from EEPROM.");
    alarm_timer.load_schedule(alarm_data.schedule);
  } else {
    log_core0.info("Using empty alarm schedule.");
  }

  // Set callback for future schedule changes to be saved to EEPROM
  alarm_timer.set_schedule_change_callback(on_schedule_change);
  alarm_timer.set_callbacks(on_timer_on, on_timer_off);

  // --- Launch Network Stack on Core 1 ---
  using BoilerNetworkCore =
      network_core::NetworkCore<board::NetSocket1, board::NetSocket2,
                                board::NetSocket3, board::NetSocket4,
                                board::Timer, board::Multicore,
                                common::Logger<Console>>;
  BoilerNetworkCore::set_logger(&log_core1);

  log_core0.info("Launching Network Stack on Core 1...");
  board::Multicore::launch_core1(BoilerNetworkCore::core1_main);

  // --- Set up IPC and Network ---
  ipc.set_mqtt_root_name("boiler");
  ipc.set_mqtt_msg_cb(on_mqtt_message);
  ipc.set_link_up_cb(on_link_up);
  ipc.set_link_down_cb(on_link_down);
  ipc.set_network_up_cb(on_network_up);
  ipc.set_network_down_cb(on_network_down);
  ipc.set_mqtt_connected_cb(on_mqtt_connected);
  ipc.set_mqtt_disconnected_cb(on_mqtt_disconnected);

  // --- Discover 1-Wire Sensors ---
  log_core0.info("Scanning for 1-Wire DS18B20 sensors...");
  TempSensorBus::SearchState search_state;
  uint8_t discovered_roms[10][8]; // Store up to 10 sensors
  int sensor_count = 0;

  while (TempSensorBus::search(search_state) && sensor_count < 10) {
    for (int i = 0; i < 8; i++) {
      discovered_roms[sensor_count][i] = search_state.rom[i];
    }
    log_core0.printf(
        "[INF] Found Sensor %d: %02X%02X%02X%02X%02X%02X%02X%02X\r\n",
        sensor_count + 1, search_state.rom[7], search_state.rom[6],
        search_state.rom[5], search_state.rom[4], search_state.rom[3],
        search_state.rom[2], search_state.rom[1], search_state.rom[0]);
    sensor_count++;
  }

  if (sensor_count == 0) {
    log_core0.warn("No DS18B20 sensors found on the 1-Wire bus!");
    // Draw empty status to display
  } else {
    log_core0.printf("[INF] Found a total of %d sensors.\r\n", sensor_count);
  }

  // --- Initialize Temperature Manager ---
  for (int i = 0; i < sensor_count; ++i) {
    const boiler::SensorConfig *config =
        boiler::find_sensor_config(discovered_roms[i]);
    if (config) {
      temp_manager.add_sensor(discovered_roms[i]);
      log_core0.printf("[INF] Registered Sensor %d: %s to TempManager.\r\n",
                       i + 1, config->name);
    } else {
      log_core0.printf("[WRN] Sensor %02X%02X... not in config.\r\n",
                       discovered_roms[i][7], discovered_roms[i][6]);
    }
  }
  temp_manager.set_on_change(on_temp_change);
  temp_manager.set_on_failure(on_temp_failure);
  temp_manager.init();

  // --- Initialize Menu System ---
  boiler::setup_menu(menu_system);

  // --- Initialize Burner Control ---
  burner_control.init();

  // --- Main loop ---
  bool last_active = false;
  uint8_t last_second = 0;

  for (;;) {
    uint32_t now = board::Timer::ticks_ms();

    // Process IPC Messages from Core 1
    ipc.process_messages();

    // Update Temperature Manager
    temp_manager.update();

    // one second timer
    if (SysClock::get_current_time().sec != last_second) {
      last_second = SysClock::get_current_time().sec;

      // Update Burner Control
      const boiler::TemperatureSensor *boiler_sensor =
          temp_manager.get_sensor_by_name("boiler_core");
      float current_boiler_temp =
          boiler_sensor ? boiler_sensor->temp_c : 100.0f;
      bool is_valid = boiler_sensor ? boiler_sensor->is_valid : false;
      int target_temp = app_settings.get_boiler_temp();
      burner_control.update(current_boiler_temp, target_temp, is_valid, now);

      // Alarm update time update
      datetime_t current_time = SysClock::get_current_time();

      common::DateTime alarm_dt;
      alarm_dt.year = current_time.year;
      alarm_dt.month = current_time.month;
      alarm_dt.day = current_time.day;
      alarm_dt.hour = current_time.hour;
      alarm_dt.minute = current_time.min;
      alarm_dt.second = current_time.sec;
      alarm_dt.day_of_week = current_time.dotw; // 0=Sunday
      alarm_timer.update_time(alarm_dt);

      // Update Home Screen
      if (!menu_system.is_active()) {
        if (last_active) {
          LcdHat::Display::clear_screen();
          last_active = false;
        }
        display_home_screen();
      } else {
        last_active = true;
      }
    }

    // Capture Joystick Edge Triggers (Naively assuming continuous checks for
    // now, real implementations should track previous state for rising edge
    // detection)
    static bool prev_up = false;
    static bool prev_down = false;
    static bool prev_left = false;
    static bool prev_right = false;
    static bool prev_select = false;

    bool cur_up = LcdHat::Joystick::up();
    bool cur_down = LcdHat::Joystick::down();
    bool cur_left = LcdHat::Joystick::left();
    bool cur_right = LcdHat::Joystick::right();
    bool cur_select = LcdHat::Joystick::select();

    if (cur_select && !prev_select)
      menu_system.on_select();
    if (cur_up && !prev_up)
      menu_system.on_up();
    if (cur_down && !prev_down)
      menu_system.on_down();
    if (cur_left && !prev_left)
      menu_system.on_left();
    if (cur_right && !prev_right)
      menu_system.on_right();

    prev_up = cur_up;
    prev_down = cur_down;
    prev_left = cur_left;
    prev_right = cur_right;
    prev_select = cur_select;

    // Redraw menu every loop if active
    if (menu_system.is_active()) {
      menu_system.draw<LcdHat::Display>();
    }

    board::Timer::sleep_ms(10);
  }
}
