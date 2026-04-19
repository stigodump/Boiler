#include "mqtt_handler.hpp"
#include "boiler_board.hpp"
#include "common/logger.hpp"
#include "common/nmea_zda.hpp"
#include "homeassistant_mqtt.hpp"
#include <cstring>

extern common::Logger<boiler_board::Console> log_core0;

void on_mqtt_message(const char *topic, const uint8_t *payload, size_t len) {
  // Delegate to Home Assistant MQTT module first
  if (boiler::ha_mqtt.handle_message(topic, payload, len)) {
    return;
  }

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
      boiler_board::SysClock::set_time(dt);

    } else {
      log_core0.warn("[MQTT] Failed to parse ZDA string");
    }
  } else if (strncmp(topic, "home/boiler/set", 15) == 0) {
    // Process boiler set commands (e.g., target temp, manual override)
    log_core0.info("[MQTT] Received boiler set command");
  }
}
