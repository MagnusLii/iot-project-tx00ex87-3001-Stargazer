/**
 * @file clock.cpp
 * @brief Implementation of the Clock class for RTC timekeeping and alarms.
 */

#include "clock.hpp"

#include "debug.hpp"

/**
 * @brief Pointer to the active Clock instance.
 */
static Clock *clock_inst = nullptr;

/**
 * @brief Constructs a Clock object and initializes the RTC.
 */
Clock::Clock() {
    clock_inst = this;
    rtc_init();
}

/**
 * @brief Updates the clock with a given timestamp string.
 * @param str String representation of a timestamp.
 * @note Helper function for update(time_t timestamp).
 */
void Clock::update(std::string &str) {
    size_t converted;
    if (time_t timestamp = std::stoll(str, &converted); converted == str.size()) { update(timestamp); }
}

/**
 * @brief Updates the RTC with a given timestamp.
 * @param timestamp Unix timestamp to set the clock.
 */
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

    datetime_t now = {.year = year, .month = month, .day = day, .hour = hour, .min = min, .sec = sec};

    DEBUG("Received time: ", now.year, "-", unsigned(now.month), "-", unsigned(now.day), " ", unsigned(now.hour), ":",
          unsigned(now.min), ":", unsigned(now.sec));

    if (rtc_set_datetime(&now)) {
        DEBUG("TIME SYNCED");
        synced = true;
    } else {
        DEBUG("TIME NOT SYNCED");
    }
}

/**
 * @brief Retrieves the current datetime from the RTC.
 * @return Current datetime_t structure.
 */
datetime_t Clock::get_datetime() const {
    datetime_t now;
    rtc_get_datetime(&now);
    return now;
}

/**
 * @brief Checks if the RTC time has been successfully synchronized.
 * @return True if synchronized, otherwise false.
 */
bool Clock::is_synced() const { return synced; }

/**
 * @brief Alarm handler function triggered when the alarm rings.
 */
void alarm_handler() {
    DEBUG("ALARM RINGING");
    clock_inst->alarm_wakeup = true;
}

/**
 * @brief Sets an alarm for a given datetime.
 * @param datetime The datetime at which the alarm should trigger.
 */
void Clock::add_alarm(datetime_t datetime) { rtc_set_alarm(&datetime, &alarm_handler); }

/**
 * @brief Checks if the alarm is currently ringing.
 * @return True if the alarm is active, otherwise false.
 */
bool Clock::is_alarm_ringing() const { return alarm_wakeup; }

/**
 * @brief Clears the active alarm.
 */
void Clock::clear_alarm() {
    rtc_disable_alarm();
    alarm_wakeup = false;
}
