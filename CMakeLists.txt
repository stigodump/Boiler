# Main application targets are defined here

# ---------------------------------------------------------------------------
# App Targets
# ---------------------------------------------------------------------------

# Standalone Build (Runs directly from default flash memory, no bootloader needed)
add_executable(boiler_standalone src/main.cpp src/mqtt_handler.cpp src/eeprom_manager.cpp src/ipc_handler.cpp src/temp_manager.cpp src/menu_system.cpp src/boiler_menu.cpp src/app_settings.cpp src/burner_control.cpp src/homeassistant_mqtt.cpp)
target_include_directories(boiler_standalone PRIVATE include)
target_link_libraries(boiler_standalone baseapp pico_multicore hardware_rtc)
pico_add_extra_outputs(boiler_standalone)
pico_enable_stdio_usb(boiler_standalone 0)
pico_enable_stdio_uart(boiler_standalone 0)

# Slot A Build
add_executable(boiler_slot_a src/main.cpp src/mqtt_handler.cpp src/eeprom_manager.cpp src/ipc_handler.cpp src/temp_manager.cpp src/menu_system.cpp src/boiler_menu.cpp src/app_settings.cpp src/burner_control.cpp src/homeassistant_mqtt.cpp)
target_include_directories(boiler_slot_a PRIVATE include)
target_link_libraries(boiler_slot_a baseapp pico_multicore hardware_rtc)
pico_set_linker_script(boiler_slot_a ${CMAKE_CURRENT_SOURCE_DIR}/memmap_slot_a.ld)
pico_add_extra_outputs(boiler_slot_a)
pico_enable_stdio_usb(boiler_slot_a 0)
pico_enable_stdio_uart(boiler_slot_a 0)

# Slot B Build
add_executable(boiler_slot_b src/main.cpp src/mqtt_handler.cpp src/eeprom_manager.cpp src/ipc_handler.cpp src/temp_manager.cpp src/menu_system.cpp src/boiler_menu.cpp src/app_settings.cpp src/burner_control.cpp src/homeassistant_mqtt.cpp)
target_include_directories(boiler_slot_b PRIVATE include)
target_link_libraries(boiler_slot_b baseapp pico_multicore hardware_rtc)
pico_set_linker_script(boiler_slot_b ${CMAKE_CURRENT_SOURCE_DIR}/memmap_slot_b.ld)
pico_add_extra_outputs(boiler_slot_b)
pico_enable_stdio_usb(boiler_slot_b 0)
pico_enable_stdio_uart(boiler_slot_b 0)
