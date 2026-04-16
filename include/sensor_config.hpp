#pragma once

#include <cstdint>

namespace boiler {

/// @brief Configuration mapping for a specific DS18B20 sensor.
struct SensorConfig {
  uint8_t rom[8];
  const char *name;
  int display_index; // Used to order the sensors on the display (0, 1, 2, etc.)
};

// Example hardcoded sensor configurations. You can replace the ROMs with your
// actual sensor ROM ids!
inline const SensorConfig known_sensors[] = {
    {{0x28, 0xB2, 0xAF, 0x56, 0x57, 0x23, 0x0B, 0x4F}, "boiler_core_0", 0},
    {{0x28, 0xF9, 0xE4, 0x55, 0x57, 0x23, 0x0B, 0xD3}, "boiler_core_1", 1},
    {{0x28, 0xF9, 0xE4, 0x55, 0x57, 0x23, 0x0B, 0xD4}, "boiler_return", 2},
    {{0x28, 0xF9, 0xE4, 0x55, 0x57, 0x23, 0x0B, 0xD5}, "boiler_system", 3},
};

inline const int num_known_sensors =
    sizeof(known_sensors) / sizeof(known_sensors[0]);

/**
 * @brief Helper to find a sensor's config by its 8-byte ROM.
 * @return Pointer to the configuration, or nullptr if not found.
 */
inline const SensorConfig *find_sensor_config(const uint8_t *rom) {
  for (int i = 0; i < num_known_sensors; ++i) {
    bool match = true;
    for (int j = 0; j < 8; ++j) {
      if (known_sensors[i].rom[j] != rom[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      return &known_sensors[i];
    }
  }
  return nullptr; // Unknown sensor
}

} // namespace boiler
