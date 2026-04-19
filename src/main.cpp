#include "common/alarm_timer.hpp"
#include "common/fonts/font10x16.hpp"
#include "common/logger.hpp"

#include "app_settings.hpp"
#include "boiler_board.hpp"
#include "boiler_menu.hpp"
#include "burner_control.hpp"
#include "clock_manager.hpp"
#include "eeprom_manager.hpp"
#include "homeassistant_mqtt.hpp"
#include "menu_system.hpp"
#include "mqtt_handler.hpp"
#include "platform/ipc_handler.hpp"
#include "sensor_config.hpp"
#include "temp_manager.hpp"

using namespace boiler_board;

// Global loggers
common::Logger<Console> log_core0;

// IPC Handler
ipc::IpcHandler<common::Logger<Console>> ipc_ctrl;

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

// Display Home Screen
void display_home_screen(bool clear_screen = false) {
  if (menu_system.is_active()) {
    return;
  }
  const common::fonts::FontDef<uint16_t> &font = common::fonts::Font10x16_Def;
  const uint8_t scale = 1;
  const uint8_t char_width = (font.width + 1) * scale;
  const uint8_t char_height = (font.height + 2) * scale;
  const uint8_t scn_width_chars = LcdHat::Display::ScreenWidth / char_width;

  datetime_t current_time = SysClock::get_current_time();
  char temp_str[64];

  auto draw_centered_string = [&](const char *str, uint8_t y,
                                  uint16_t color = 0xFFFF) -> void {
    char cntr_str[64];
    snprintf(cntr_str, sizeof(cntr_str), "%*s%s%*s",
             (scn_width_chars - strlen(str)) / 2, "", str,
             (scn_width_chars - strlen(str)) / 2, "");
    LcdHat::Display::draw_string(0, y, cntr_str, color, 0x0000, font, scale);
  };

  if (clear_screen) {
    LcdHat::Display::clear_screen();
  }

  // --- Draw Time ---
  snprintf(temp_str, sizeof(temp_str), "%02d:%02d:%02d", current_time.hour,
           current_time.min, current_time.sec);
  draw_centered_string(temp_str, 15);

  // --- Draw Date ---
  snprintf(temp_str, sizeof(temp_str), "%02d-%02d-%04d", current_time.day,
           current_time.month, current_time.year);
  draw_centered_string(temp_str, 40);

  // Display Boiler Temp
  float calc_temp = burner_control.get_calculated_temp();
  bool degraded = burner_control.is_degraded();

  if (calc_temp > -99.0f) {
    snprintf(temp_str, sizeof(temp_str),
             "Boiler%s: %04.1f\x7F"
             "C",
             degraded ? "!" : "", calc_temp);
    draw_centered_string(temp_str, 80, degraded ? 0xF800 : 0xFFFF);
  } else {
    snprintf(temp_str, sizeof(temp_str), "Boiler: Wait/Err");
    draw_centered_string(temp_str, 80, 0xF800);
  }

  // Display Alarm Timer Status
  alarm_timer.get_transition_str(temp_str, sizeof(temp_str));
  snprintf(temp_str, sizeof(temp_str), "%s", temp_str);
  draw_centered_string(temp_str, 110, 0x07E0);

  // Display Burner Status
  if (burner_control.is_burner_on()) {
    snprintf(temp_str, sizeof(temp_str), "Burner: ON");
  } else {
    snprintf(temp_str, sizeof(temp_str), "Burner: OFF");
  }

  draw_centered_string(temp_str, 140, 0x07E0);

  // Display Pump Status
  if (burner_control.is_pump_on()) {
    snprintf(temp_str, sizeof(temp_str), "Pump: ON");
  } else {
    snprintf(temp_str, sizeof(temp_str), "Pump: OFF");
  }

  draw_centered_string(temp_str, 170, 0x07E0);
}

// Burner control callbacks
void on_burner_state_change(boiler::BurnerControl::State new_state,
                            uint32_t on_time_sec) {
  if (new_state == boiler::BurnerControl::State::Off) {
    app_settings.add_burner_runtime(on_time_sec);
  }
  display_home_screen(false);
}

// Control timer callbacks
void on_timer_on() {
  log_core0.info("[APP] Alarm Timer triggered ON event!");
  burner_control.set_auto_mode(true);
  display_home_screen(false);
}

void on_timer_off() {
  log_core0.info("[APP] Alarm Timer triggered OFF event!");
  burner_control.set_auto_mode(false);
  display_home_screen(false);
}

void on_timer_mode_change(common::TimerMode mode) {
  display_home_screen(false);
}

// Network Status callbacks
void on_link_up() { board::Led::set(true); }
void on_link_down() { board::Led::set(false); }
void on_network_up() {}
void on_network_down() {}

// MQTT callbacks
void on_mqtt_connected() {
  ipc_ctrl.subscribe_topic("/weather_station/gnss/telegram/ZDA");
  board::Timer::sleep_ms(
      200); // Required purely pacing for W5100s buffer flush (QoS 0 drops)
  boiler::ha_mqtt.publish_discovery();
  board::Timer::sleep_ms(200);
  boiler::ha_mqtt.subscribe_topics();
}
void on_mqtt_disconnected() {}

// Temperature Manager callbacks
void on_temp_change(const boiler::TemperatureSensor *sensor) {
  if (!sensor || !sensor->sensor_id) {
    log_core0.warn("[APP] Received change callback for invalid sensor.");
    return;
  }

  // Publish telemetry
  if (ipc_ctrl.get_status().is_mqtt_connected) {
    char payload[sizeof(ipc::MqttMessage::payload)];
    char topic[sizeof(ipc::MqttMessage::topic)];
    snprintf(topic, sizeof(topic), "temperature/%s", sensor->sensor_id->name);
    snprintf(payload, sizeof(payload), "%.2f", sensor->temp_c);
    ipc_ctrl.publish_telemetry(topic, payload);
  }
}

// Calculated temperature change callback
void on_calculated_temp_change(float temp) {
  display_home_screen(false);

  // Publish calculated telemetry
  if (ipc_ctrl.get_status().is_mqtt_connected) {
    char payload[sizeof(ipc::MqttMessage::payload)];
    char topic[sizeof(ipc::MqttMessage::topic)];
    snprintf(topic, sizeof(topic), "temperature/boiler_core_calc");
    snprintf(payload, sizeof(payload), "%.2f", temp);
    ipc_ctrl.publish_telemetry(topic, payload);
  }
}

void on_temp_failure(const boiler::TemperatureSensor *sensor) {
  if (!sensor || !sensor->sensor_id) {
    log_core0.warn("[APP] Received failure callback for invalid sensor.");
    return;
  }

  log_core0.error("[APP] %s FAIL (count: %d)", sensor->sensor_id->name,
                  sensor->fail_count);
}

void on_schedule_change(const common::AlarmTimer::Schedule &slots) {
  log_core0.info("[APP] Schedule changed");
  boiler::AlarmSettingsData data;
  for (int d = 0; d < 7; ++d) {
    for (int i = 0; i < common::AlarmTimer::kMaxSlotsPerDay; ++i) {
      data.schedule[d][i] = slots[d][i];
    }
  }
  boiler::eeprom_manager::save_alarm_schedule(data);
  display_home_screen(false);
}

int main() {
  // Start the watchdog immediately — the first thing the app does.
  // Core 1's main loop kicks it every 10 ms once running, so the 8-second
  // window covers all legitimate startup paths. Any crash before Core 1
  // reaches its kick loop causes a reboot and bootloader rollback.
  board::Watchdog::enable(8000);

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
  Console::init();

  // --- Initialize Temperature Sensor ---
  TempSensor::init();

  // --- Initialize Display & Joystick  ---
  LcdHat::init();
  LcdHat::Display::set_backlight(true);

  log_core0.info("[APP] Boiler System Starting on Core 0...");

  // --- Load Settings from EEPROM (must be after I2C + Console init) ---
  app_settings.load();

  // --- Initialize Alarm Timer ---
  alarm_timer.init();

  // --- Load Alarm Schedule from EEPROM ---
  boiler::AlarmSettingsData alarm_data;
  if (boiler::eeprom_manager::load_alarm_schedule(alarm_data)) {
    log_core0.info("[APP] Loaded alarm schedule from EEPROM.");
    alarm_timer.load_schedule(alarm_data.schedule);
  } else {
    log_core0.info("[APP] Using empty alarm schedule.");
  }

  // Set callback for future schedule changes to be saved to EEPROM
  alarm_timer.set_schedule_change_callback(on_schedule_change);
  alarm_timer.set_callbacks(on_timer_on, on_timer_off);
  alarm_timer.set_mode_change_callback(on_timer_mode_change);

  // Set callback for burner state changes
  burner_control.set_state_change_callback(on_burner_state_change);
  burner_control.set_temp_change_callback(on_calculated_temp_change);

  using BoilerNetworkCore = network_core::NetworkCore<
      board::NetSocket1, board::NetSocket2, board::NetSocket3,
      board::NetSocket4, board::Timer, board::Multicore, board::Watchdog>;

  log_core0.info("[APP] Launching Network Stack on Core 1...");
  board::Multicore::launch_core1(BoilerNetworkCore::core1_main);

  ipc_ctrl.set_logger_cb([](const char *msg) { Console::write_str(msg); });
  ipc_ctrl.set_mqtt_root_name("boiler");
  ipc_ctrl.set_mqtt_msg_cb(on_mqtt_message);
  ipc_ctrl.set_link_up_cb(on_link_up);
  ipc_ctrl.set_link_down_cb(on_link_down);
  ipc_ctrl.set_network_up_cb(on_network_up);
  ipc_ctrl.set_network_down_cb(on_network_down);
  ipc_ctrl.set_mqtt_connected_cb(on_mqtt_connected);
  ipc_ctrl.set_mqtt_disconnected_cb(on_mqtt_disconnected);

  // --- Discover 1-Wire Sensors ---
  log_core0.info("[APP] Scanning for 1-Wire DS18B20 sensors...");
  TempSensorBus::SearchState search_state;
  uint8_t discovered_roms[10][8]; // Store up to 10 sensors
  int sensor_count = 0;

  while (TempSensorBus::search(search_state) && sensor_count < 10) {
    for (int i = 0; i < 8; i++) {
      discovered_roms[sensor_count][i] = search_state.rom[i];
    }
    log_core0.info("[APP] Found Sensor %d: %02X%02X%02X%02X%02X%02X%02X%02X",
                   sensor_count + 1, search_state.rom[7], search_state.rom[6],
                   search_state.rom[5], search_state.rom[4],
                   search_state.rom[3], search_state.rom[2],
                   search_state.rom[1], search_state.rom[0]);
    sensor_count++;
  }

  if (sensor_count == 0) {
    log_core0.warn("[APP] No DS18B20 sensors found on the 1-Wire bus!");
    // Draw empty status to display
  } else {
    log_core0.info("[APP] Found a total of %d sensors.", sensor_count);
  }

  // --- Initialize Temperature Manager ---
  for (int i = 0; i < sensor_count; ++i) {
    const boiler::SensorConfig *config =
        boiler::find_sensor_config(discovered_roms[i]);
    if (config) {
      temp_manager.add_sensor(discovered_roms[i]);
      log_core0.info("[APP] Registered Sensor %d: %s to TempManager.", i + 1,
                     config->name);
    } else {
      log_core0.warn("[APP] Sensor %02X%02X... not in config.",
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
  bool last_active = true;
  uint8_t last_second = 0;
  uint32_t last_joystick_activity_ms = 0;

  for (;;) {
    uint32_t now = board::Timer::ticks_ms();

    // Process IPC Messages from Core 1
    ipc_ctrl.process_messages();

    // Update Temperature Manager
    temp_manager.update();

    // one second timer
    if (SysClock::get_current_time().sec != last_second) {
      last_second = SysClock::get_current_time().sec;

      // Update Burner Control
      const boiler::TemperatureSensor *boiler_sensor_0 =
          temp_manager.get_sensor_by_name("boiler_core_0");
      const boiler::TemperatureSensor *boiler_sensor_1 =
          temp_manager.get_sensor_by_name("boiler_core_1");

      float current_boiler_temp_0 =
          boiler_sensor_0 ? boiler_sensor_0->temp_c : -100.0f;
      bool is_valid_0 = boiler_sensor_0 ? boiler_sensor_0->is_valid : false;

      float current_boiler_temp_1 =
          boiler_sensor_1 ? boiler_sensor_1->temp_c : -100.0f;
      bool is_valid_1 = boiler_sensor_1 ? boiler_sensor_1->is_valid : false;

      int target_temp = app_settings.get_boiler_temp();
      uint32_t overrun_sec = app_settings.get_pump_run_time_sec();
      burner_control.update(current_boiler_temp_0, is_valid_0,
                            current_boiler_temp_1, is_valid_1, target_temp, now,
                            overrun_sec);

      // Publish HA state using calculated temperature
      if (ipc_ctrl.get_status().is_mqtt_connected) {
        float calc_temp = burner_control.get_calculated_temp();
        const char *mode = burner_control.get_auto_mode() ? "auto" : "off";
        const char *action = burner_control.is_burner_on() ? "heating" : "idle";
        boiler::ha_mqtt.publish_state(calc_temp > -99.0f ? calc_temp : 0.0f,
                                      (float)target_temp, mode, action);
      }

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
          display_home_screen(true);
          last_active = false;
        } else {
          display_home_screen();
        }
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

    bool any_press = (cur_select && !prev_select) || (cur_up && !prev_up) ||
                     (cur_down && !prev_down) || (cur_left && !prev_left) ||
                     (cur_right && !prev_right);
    if (any_press) {
      last_joystick_activity_ms = now;
    }

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
      if ((now - last_joystick_activity_ms) > 60000) {
        menu_system.close();
      } else {
        menu_system.draw<LcdHat::Display>();
      }
    }

    // Signal Core 1 that Core 0 is alive — Core 1 gates its watchdog kick on
    // this heartbeat.  A sustained Core 0 hang will cause the watchdog to
    // expire and the bootloader to invoke its rollback logic.
    BoilerNetworkCore::signal_core0_alive();

    board::Timer::sleep_ms(10);
  }
}
