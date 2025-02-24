#include "clock.hpp"

#include "debug.hpp"
#include <hardware/rtc.h>


static Clock* clock_inst = nullptr;

Clock::Clock() {
    clock_inst = this;
    rtc_init();
}

void Clock::update(std::string &str) {
    size_t converted;
    if (time_t timestamp = std::stoll(str, &converted); converted == str.size()) {
        update(timestamp);
    }
}

void Clock::update(time_t timestamp) {
    synced = false;
    last_timestamp = timestamp;

    struct tm *timeinfo = gmtime(&timestamp);

    int16_t year = static_cast<int16_t>(timeinfo->tm_year + 1900);
    int8_t month = static_cast<int8_t>(timeinfo->tm_mon + 1);
    int8_t day = static_cast<int8_t>(timeinfo->tm_mday);
    int8_t hour = static_cast<int8_t>(timeinfo->tm_hour);
    int8_t min = static_cast<int8_t>(timeinfo->tm_min);
    int8_t sec = static_cast<int8_t>(timeinfo->tm_sec);

    datetime_t now = {
        .year = year,
        .month = month,
        .day = day,
        .hour = hour,
        .min = min,
        .sec = sec
    };

    DEBUG("Received time: ", now.year, "-", unsigned(now.month), "-", unsigned(now.day), " ", unsigned(now.hour), ":", unsigned(now.min), ":", unsigned(now.sec));

    if (rtc_set_datetime(&now)) {
        DEBUG("TIME SYNCED");
        synced = true;
    } else {
        DEBUG("TIME NOT SYNCED");
    }

}

datetime_t Clock::get_datetime() {
    datetime_t now;
    rtc_get_datetime(&now);
    return now;
}

bool Clock::is_synced() {
    return synced;
}

void alarm_handler() {
    DEBUG("ALARM RINGING");
    clock_inst->alarm_wakeup = true;
}

void Clock::add_alarm(datetime_t datetime) {
    rtc_set_alarm(&datetime, &alarm_handler);
}

bool Clock::is_alarm_ringing() {
    return alarm_wakeup;
}

void Clock::clear_alarm() {
    rtc_disable_alarm();
    alarm_wakeup = false;
}
