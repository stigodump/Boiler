#include "ipc_handler.hpp"
#include "boiler_board.hpp"
#include "common/logger.hpp"
#include <cstring>

extern common::Logger<boiler_board::Console> log_core0;

namespace boiler {

IpcHandler::IpcHandler() { memset(&status_, 0, sizeof(status_)); }

static void __not_in_flash_func(ipc_ram_pause_loop)() {
  using namespace network_core::ipc;

  // Direct HW Register pointers to bypass any compiler linking to XIP flash
  volatile uint32_t *sio_fifo_st = (volatile uint32_t *)0xd0000050; // SIO_FIFO_ST
  volatile uint32_t *sio_fifo_rd = (volatile uint32_t *)0xd0000058; // SIO_FIFO_RD
  volatile uint32_t *sio_fifo_wr = (volatile uint32_t *)0xd0000054; // SIO_FIFO_WR

  while (true) {
    if ((*sio_fifo_st) & 0x01) { // SIO_FIFO_ST_VLD_BITS
      uint32_t raw = *sio_fifo_rd;
      
      // Safely echo Pico SDK's native hardware lockout ping sequences blindly 
      // back to Core 1 without exiting the RAM loop to satisfy flash_range_erase
      if ((raw & 0xFFFFFF00) == 0x73a88300) {
          while (!((*sio_fifo_st) & 0x02)); // WAIT FOR SIO_FIFO_ST_RDY_BITS
          *sio_fifo_wr = raw;
          continue;
      }
      
      // Ensure we explicitly catch the End command, skipping anything else
      Message *msg = reinterpret_cast<Message *>(raw);
      if (msg && msg->type == MsgType::OtaFlashLockoutEnd) {
        break; // Free to return to XIP Flash safely!
      }
    }
  }
}

void IpcHandler::process_messages() {
  using namespace boiler_board;
  using namespace network_core::ipc;

  while (board::Multicore::fifo_rvalid()) {
    uint32_t raw_ptr = board::Multicore::fifo_pop_blocking();
    Message *msg = reinterpret_cast<Message *>(raw_ptr);

    if (!msg)
      break;

    switch (msg->type) {
    case MsgType::LinkUp:
      log_core0.info("[IPC] Link Up");
      status_.is_link_up = true;
      if (link_up_cb_)
        link_up_cb_();
      break;
    case MsgType::LinkDown:
      log_core0.info("[IPC] Link Down");
      status_.is_link_up = false;
      if (link_down_cb_)
        link_down_cb_();
      break;
    case MsgType::NetworkUp:
      log_core0.info("[IPC] Network Up");
      status_.is_network_up = true;
      if (network_up_cb_)
        network_up_cb_();
      break;
    case MsgType::NetworkDown:
      log_core0.info("[IPC] Network Down");
      status_.is_network_up = false;
      if (network_down_cb_)
        network_down_cb_();
      break;
    case MsgType::MqttConnected:
      log_core0.info("[IPC] MQTT Connected");
      status_.is_mqtt_connected = true;
      if (mqtt_connected_cb_)
        mqtt_connected_cb_();
      break;
    case MsgType::MqttDisconnected:
      log_core0.info("[IPC] MQTT Disconnected");
      status_.is_mqtt_connected = false;
      if (mqtt_disconnected_cb_)
        mqtt_disconnected_cb_();
      break;
    case MsgType::MqttMessageReceived:
      if (mqtt_msg_cb_) {
        mqtt_msg_cb_(msg->mqtt_msg.topic, msg->mqtt_msg.payload,
                     msg->mqtt_msg.payload_len);
      }
      break;
    case MsgType::OtaFlashLockoutStart:
      log_core0.info("[IPC] Received OTA Lockout Request. Moving to SRAM...");
      
      // Acknowledge the lockout request to allow Core 1 to start erasing
      board::Multicore::fifo_push_blocking(0xDEADBEEF);

      // Execute exactly from SRAM to spin until told to wake up
      ipc_ram_pause_loop();
      
      log_core0.info("[IPC] OTA Lockout Ended. Resuming execution from Flash.");
      break;
    default:
      break;
    }
  }
}

void IpcHandler::publish_telemetry(const char *topic, const char *payload, bool retain) {
  using namespace boiler_board;
  using namespace network_core::ipc;

  if (!status_.is_mqtt_connected || !board::Multicore::fifo_wready())
    return;

  Message *tx_msg = tx_pool_.alloc();
  tx_msg->type = MsgType::MqttPublish;
  tx_msg->mqtt_msg.retain = retain;

  strncpy(tx_msg->mqtt_msg.topic, topic, sizeof(tx_msg->mqtt_msg.topic) - 1);

  size_t len = strlen(payload);
  if (len > sizeof(tx_msg->mqtt_msg.payload))
    len = sizeof(tx_msg->mqtt_msg.payload);

  memcpy(tx_msg->mqtt_msg.payload, payload, len);
  tx_msg->mqtt_msg.payload_len = len;

  board::Multicore::fifo_push_blocking(reinterpret_cast<uint32_t>(tx_msg));
}

void IpcHandler::subscribe_topic(const char *topic) {
  using namespace boiler_board;
  using namespace network_core::ipc;

  if (!board::Multicore::fifo_wready())
    return;

  Message *msg = tx_pool_.alloc();
  msg->type = MsgType::MqttSubscribe;
  msg->mqtt_msg.payload_len = 0;
  strncpy(msg->mqtt_msg.topic, topic, sizeof(msg->mqtt_msg.topic) - 1);
  board::Multicore::fifo_push_blocking(reinterpret_cast<uint32_t>(msg));
}

void IpcHandler::set_mqtt_root_name(const char *root_name) {
  using namespace boiler_board;
  using namespace network_core::ipc;

  if (!board::Multicore::fifo_wready())
    return;

  Message *msg = tx_pool_.alloc();
  msg->type = MsgType::SetMqttRootName;
  strncpy(msg->mqtt_root_name.root_name, root_name,
          sizeof(msg->mqtt_root_name.root_name) - 1);
  msg->mqtt_root_name.root_name[sizeof(msg->mqtt_root_name.root_name) - 1] =
      '\0';
  board::Multicore::fifo_push_blocking(reinterpret_cast<uint32_t>(msg));
}

} // namespace boiler
