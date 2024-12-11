#include "clock.hpp"

#include "debug.hpp"
#include <pico/time.h>

Clock::Clock() {
    rtc_init();
}

void Clock::update(time_t timestamp) {
    synced = false;
    last_timestamp = timestamp;

    struct tm *timeinfo = gmtime(&timestamp);

    DEBUG("TIME: ", timeinfo->tm_year + 1900, " ", timeinfo->tm_mon + 1, " ", timeinfo->tm_mday, " ", timeinfo->tm_hour, " ", timeinfo->tm_min, " ", timeinfo->tm_sec);
    sleep_ms(5000);

    int16_t year = static_cast<int16_t>(timeinfo->tm_year + 1900);
    int8_t month = static_cast<int8_t>(timeinfo->tm_mon + 1);
    int8_t day = static_cast<int8_t>(timeinfo->tm_mday);
    int8_t hour = static_cast<int8_t>(timeinfo->tm_hour);
    int8_t min = static_cast<int8_t>(timeinfo->tm_min);
    int8_t sec = static_cast<int8_t>(timeinfo->tm_sec);

    DEBUG("TIME: ", year, " ", unsigned(month), " ", unsigned(day), " ", unsigned(hour), " ", unsigned(min), " ", unsigned(sec));

    sleep_ms(5000);
    datetime_t now = {
        .year = year,
        .month = month,
        .day = day,
        .hour = hour,
        .min = min,
        .sec = sec
    };

    DEBUG("TIME: ", now.year, " ", unsigned(now.month), " ", unsigned(now.day), " ", unsigned(now.hour), " ", unsigned(now.min), " ", unsigned(now.sec));
    sleep_ms(5000);

    if (rtc_set_datetime(&now)) {
        DEBUG("TIME SYNCED");
        synced = true;
    } else {
        DEBUG("TIME NOT SYNCED");
    }

    DEBUG("TIME: ", now.year, " ", unsigned(now.month), " ", unsigned(now.day), " ", unsigned(now.hour), " ", unsigned(now.min), " ", unsigned(now.sec));
}

datetime_t Clock::get_datetime() {
    datetime_t now;
    rtc_get_datetime(&now);
    return now;
}

bool Clock::is_synced() {
    return synced;
}
