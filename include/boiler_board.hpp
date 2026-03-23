#pragma once
// Boiler Project Board Components

#include "board.hpp"
#include "drivers/at24c32/at24c32.hpp"
#include "drivers/ds1307/ds1307.hpp"
#include "drivers/ds18b20/ds18b20.hpp"
#include "drivers/onewire/pio_onewire.hpp"
#include "drivers/round_lcd_1.28_hat/round_lcd_1.28_hat.hpp"
#include "platform/onewire.hpp"

namespace boiler_board {

// App configures which UART to use
using Console = board::Uart<0, 115200>;

using Relay1 = board::Gpio2;
using Relay2 = board::Gpio3;
using PumpRelay = board::Gpio4;
using BurnerRelay = board::Gpio5;

// Temperature Sensor definition
// 1-Wire pin definition and bus driver instantiation
using TempSensorBus =
    platform::OneWire<drivers::PioOneWire<board::Pio0, board::Gpio28>>;
using TempSensor = drivers::ds18b20::DS18B20<TempSensorBus>;

// --- Display and Joystick ---
// SPI1 on GP10(SCK), GP11(MOSI). We route MISO to GP24 (unused) to free GP12.
using LcdSpi =
    board::Spi1</*sck*/ 10, /*mosi*/ 11, /*miso*/ 24, /*baud*/ 40000000>;
using LcdCs = board::Gpio9;
using LcdDc = board::Gpio8;
using LcdRst = board::Gpio12;
using LcdBl = board::Gpio13;

// Round LCD 1.28 Hat (GC9A01 LCD + 5-way Joystick)
using LcdHat = drivers::round_lcd_128_hat::RoundLcd128Hat<
    LcdSpi, LcdCs, LcdDc, LcdRst, LcdBl, board::Timer, board::Gpio6, board::Gpio14,
    board::Gpio7, board::Gpio15,
    board::Gpio22>; // Up, Down, Left, Right, Select

// --- I2C Bus & RTC / EEPROM ---
// I2C1 used on GPIO26(SDA) and GPIO27(SCL) since they are free
using RtcI2c = board::I2c<1, 26, 27, 100000>; // I2C1, 100kHz standard mode

using DS1307Rtc = drivers::DS1307<RtcI2c>;
using Eeprom = drivers::AT24C32<RtcI2c, board::Timer>;

} // namespace boiler_board
