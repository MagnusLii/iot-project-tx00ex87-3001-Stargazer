#pragma once

#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/util/datetime.h"

#include <ctime>
#include <string>

class Clock {
  public:
    Clock();

    void update(std::string &str);
    void update(time_t timestamp);
    datetime_t get_datetime() const;
    bool is_synced() const;
    void add_alarm(datetime_t datetime);
    bool is_alarm_ringing() const;
    void clear_alarm();

  private:
    friend void alarm_handler();

  private:
    time_t last_timestamp = 0;
    bool synced = false;
    bool alarm_wakeup = false;
};
