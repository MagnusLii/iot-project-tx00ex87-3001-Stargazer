#pragma once

#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "pico/time.h"
#include <string>

class Clock {
  public:
    Clock();

    void update(std::string &str);
    void update(time_t timestamp);
    datetime_t get_datetime();
    bool is_synced();

  private:
    time_t last_timestamp = 0;
    bool synced = false;
};
