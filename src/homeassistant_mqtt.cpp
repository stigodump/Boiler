#include "homeassistant_mqtt.hpp"
#include "app_settings.hpp"
#include "boiler_board.hpp"
#include "burner_control.hpp"
#include "common/alarm_timer.hpp"
#include "common/logger.hpp"
#include "platform/network_core/ipc_handler.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern boiler::AppSettings app_settings;
extern boiler::BurnerControl burner_control;
extern network_core::ipc::IpcHandler ipc;
extern common::Logger<boiler_board::Console> log_core0;
extern common::AlarmTimer alarm_timer;

namespace boiler {

HomeAssistantMqtt ha_mqtt;

void HomeAssistantMqtt::subscribe_topics() {
  ipc.subscribe_topic("/homeassistant/climate/boiler/mode/set");
  ipc.subscribe_topic("/homeassistant/climate/boiler/target/set");
  ipc.subscribe_topic("/homeassistant/climate/boiler/schedule/set");
}

void HomeAssistantMqtt::publish_discovery() {
  const char *topic = "/homeassistant/climate/boiler/config";

  // Create JSON payload for climate entity
  char payload[700];
  snprintf(payload, sizeof(payload),
           "{"
           "\"name\":\"Boiler\","
           "\"unique_id\":\"boiler_climate\","
           "\"modes\":[\"off\",\"heat\",\"auto\"],"
           "\"mode_stat_t\":\"homeassistant/climate/boiler/state\","
           "\"mode_val_tpl\":\"{{value_json.mode}}\","
           "\"mode_cmd_t\":\"homeassistant/climate/boiler/mode/set\","
           "\"curr_temp_t\":\"homeassistant/climate/boiler/state\","
           "\"curr_temp_tpl\":\"{{value_json.current_temperature}}\","
           "\"temp_cmd_t\":\"homeassistant/climate/boiler/target/set\","
           "\"temp_stat_t\":\"homeassistant/climate/boiler/state\","
           "\"temp_stat_tpl\":\"{{value_json.target_temperature}}\","
           "\"temp_step\":1,"
           "\"min_temp\":30,"
           "\"max_temp\":85,"
           "\"act_t\":\"homeassistant/climate/boiler/state\","
           "\"act_tpl\":\"{{value_json.action}}\""
           "}");

  ipc.publish_telemetry(topic, payload, true);
  log_core0.info("[HA] Published MQTT Discovery config");
}

void HomeAssistantMqtt::publish_state(float current_temp, float target_temp,
                                      const char *mode, const char *action) {
  const char *topic = "/homeassistant/climate/boiler/state";

  char payload[150];
  snprintf(payload, sizeof(payload),
           "{\"mode\":\"%s\",\"current_temperature\":%.1f,\"target_"
           "temperature\":%.1f,\"action\":\"%s\"}",
           mode, current_temp, target_temp, action);

  ipc.publish_telemetry(topic, payload, true);
}

bool HomeAssistantMqtt::handle_message(const char *topic,
                                       const uint8_t *payload, size_t len) {
  if (strncmp(topic, "homeassistant/climate/boiler/target/set", 39) == 0) {
    // Parse target temperature
    char temp_str[16];
    size_t copy_len = len < 15 ? len : 15;
    memcpy(temp_str, payload, copy_len);
    temp_str[copy_len] = '\0';

    int target = atoi(temp_str);
    if (target > 0 && target <= 100) {
      app_settings.set_boiler_temp(target);
      // Wait to publish state in the main loop or here.
      log_core0.printf("[HA] Set target temperature to %d\r\n", target);
    }
    return true;
  }

  if (strncmp(topic, "homeassistant/climate/boiler/mode/set", 37) == 0) {
    // Parse mode: "off", "heat", "auto"
    if (strncmp((const char *)payload, "off", 3) == 0) {
      alarm_timer.set_mode(common::TimerMode::AlwaysOff);
      log_core0.info("[HA] Mode set to: off");
    } else if (strncmp((const char *)payload, "auto", 4) == 0) {
      alarm_timer.set_mode(common::TimerMode::Normal);
      log_core0.info("[HA] Mode set to: auto");
    } else if (strncmp((const char *)payload, "heat", 4) == 0) {
      alarm_timer.set_mode(common::TimerMode::AlwaysOn);
      log_core0.info("[HA] Mode set to: heat");
    }
    return true;
  }

  if (strncmp(topic, "homeassistant/climate/boiler/schedule/set", 43) == 0) {
    char schedule_str[512];
    size_t copy_len = len < 511 ? len : 511;
    memcpy(schedule_str, payload, copy_len);
    schedule_str[copy_len] = '\0';

    if (alarm_timer.parse_and_load_string(schedule_str)) {
      log_core0.info("[HA] Schedule successfully loaded and synced to EEPROM");
    } else {
      log_core0.warn("[HA] Failed to parse incoming schedule string");
    }
    return true;
  }

  return false;
}

} // namespace boiler
