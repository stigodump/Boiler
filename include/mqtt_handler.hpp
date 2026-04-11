#pragma once
#include <cstddef>
#include <cstdint>

void on_mqtt_message(const char *topic, const uint8_t *payload, size_t len);
