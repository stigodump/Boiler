#pragma once

#include <cstddef>
#include <cstdint>

namespace boiler {

class HomeAssistantMqtt {
public:
  HomeAssistantMqtt() = default;

  // Subscribe to command topics. Should be called on MQTT connection.
  void subscribe_topics();

  // Publish MQTT Discovery payload. Should be called on MQTT connection.
  void publish_discovery();

  // Publish state payload. Should be called when state changes.
  // mode: "heat", "off", "auto"
  // action: "heating", "idle", "off"
  void publish_state(float current_temp, float target_temp, const char* mode, const char* action);

  // Handle incoming commands. Returns true if the message was handled.
  bool handle_message(const char* topic, const uint8_t* payload, size_t len);
};

// Global instance to be used by main.cpp
extern HomeAssistantMqtt ha_mqtt;

} // namespace boiler
