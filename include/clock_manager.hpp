#pragma once

#include "pico/util/datetime.h"

template <typename I2cRtc, typename HardwareRtc> class ClockManager {
public:
  static void init() {
    HardwareRtc::init();

    typename I2cRtc::DateTime i2c_time;
    // Read external RTC, if successful and data is sane, set the Pico internal
    // RTC
    if (I2cRtc::init() && I2cRtc::get_time(i2c_time)) {
      datetime_t dt = to_pico_datetime(i2c_time);
      HardwareRtc::set_datetime(&dt);
    }
  }

  static datetime_t get_current_time() {
    datetime_t dt;
    HardwareRtc::get_datetime(&dt);
    return dt;
  }

  static void set_time(const datetime_t &dt) {
    datetime_t current;
    HardwareRtc::get_datetime(&current);

    if (!compare_time(current, dt)) {
      // Update the Pico's internal RTC
      HardwareRtc::set_datetime(&dt);
      // Sync changes to the external I2C RTC
      I2cRtc::set_time(to_i2c_datetime(dt));
    }
  }

private:
  static bool compare_time(const datetime_t &a, const datetime_t &b) {
    return a.year == b.year && a.month == b.month && a.day == b.day &&
           a.dotw == b.dotw && a.hour == b.hour && a.min == b.min &&
           a.sec == b.sec;
  }

  static datetime_t to_pico_datetime(const typename I2cRtc::DateTime &dt) {
    datetime_t out;
    out.year = dt.year;
    out.month = dt.month;
    out.day = dt.day;
    // Typically DS1307 uses 1=Sunday, 7=Saturday
    // Pico hardware RTC uses 0=Sunday, 6=Saturday
    out.dotw = dt.day_of_week - 1;
    out.hour = dt.hour;
    out.min = dt.minute;
    out.sec = dt.second;
    return out;
  }

  static typename I2cRtc::DateTime to_i2c_datetime(const datetime_t &dt) {
    typename I2cRtc::DateTime out;
    out.year = dt.year;
    out.month = dt.month;
    out.day = dt.day;
    out.day_of_week = dt.dotw + 1;
    out.hour = dt.hour;
    out.minute = dt.min;
    out.second = dt.sec;
    return out;
  }
};
