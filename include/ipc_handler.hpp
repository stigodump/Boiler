#pragma once

#include "common/ring_pool.hpp"
#include "platform/network_core/network_core.hpp"
#include <cstddef>
#include <cstdint>

namespace boiler {

/// Callback type for incoming MQTT messages.
using MqttCallback = void (*)(const char *topic, const uint8_t *payload,
                              size_t len);

/// Callback type for status events (link, network, mqtt).
using VoidCallback = void (*)();

/// Handles IPC communication between Core 0 and Core 1 for the Boiler
/// application.
class IpcHandler {
public:
  IpcHandler();

  /// Process all pending IPC messages from Core 1.
  void process_messages();

  /// Publish telemetry to a specific MQTT topic.
  void publish_telemetry(const char *topic, const char *payload);

  /// Subscribe to an MQTT topic.
  void subscribe_topic(const char *topic);

  /// Set the root MQTT topic prefix that Core 1 will use for all msgs.
  void set_mqtt_root_name(const char *root_name);

  /// --- Callback Registrations ---
  void set_mqtt_msg_cb(MqttCallback cb) { mqtt_msg_cb_ = cb; }
  void set_link_up_cb(VoidCallback cb) { link_up_cb_ = cb; }
  void set_link_down_cb(VoidCallback cb) { link_down_cb_ = cb; }
  void set_network_up_cb(VoidCallback cb) { network_up_cb_ = cb; }
  void set_network_down_cb(VoidCallback cb) { network_down_cb_ = cb; }
  void set_mqtt_connected_cb(VoidCallback cb) { mqtt_connected_cb_ = cb; }
  void set_mqtt_disconnected_cb(VoidCallback cb) { mqtt_disconnected_cb_ = cb; }

  /// Get the current network status.
  const network_core::NetworkStatus &get_status() const { return status_; }

private:
  network_core::NetworkStatus status_;
  common::RingPool<network_core::ipc::Message, 16> tx_pool_;

  MqttCallback mqtt_msg_cb_ = nullptr;
  VoidCallback link_up_cb_ = nullptr;
  VoidCallback link_down_cb_ = nullptr;
  VoidCallback network_up_cb_ = nullptr;
  VoidCallback network_down_cb_ = nullptr;
  VoidCallback mqtt_connected_cb_ = nullptr;
  VoidCallback mqtt_disconnected_cb_ = nullptr;
};

} // namespace boiler
