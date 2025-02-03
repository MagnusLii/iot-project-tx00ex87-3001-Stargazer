#include "sleep_functions.hpp"

#include "debug.hpp"

//TODO ACTUALLY TEST IT
void sleep_for(uint hours, uint mins) {

    if (mins > 60) {
        while (mins >= 60) {
            hours++;
            mins -= 60;
        }
    }

    uint hours_ms = hours * 3.6e6;  //convert hours into milliseconds
    uint mins_ms = mins * 6e4;      //Convert minutes into milliseconds

    uint total_time = hours_ms + mins_ms;

    //Wanted to use pico extras sleep library but that apparently requires picosdk 2.0. As of now, we're using 1.5.1
    //this is just a placeholder as this only causes delay and isn't a low-energy mode
    sleep_ms(total_time);
}


//TODO ACTUALLY TEST IT
void sleep_until_certain_time(const std::shared_ptr<Clock>& clock, const datetime_t target_time) {
    if (clock->is_synced()) {
        datetime_t current_time = clock->get_datetime();

        tm tm_time_cur = {};
        tm tm_time_tar = {};

        //Convert from datetime_t to tm from time.h
        tm_time_cur.tm_year = current_time.year;
        tm_time_cur.tm_mon = current_time.month;
        tm_time_cur.tm_mday = current_time.day;
        tm_time_cur.tm_hour = current_time.hour;
        tm_time_cur.tm_min = current_time.min;

        tm_time_tar.tm_year = target_time.year;
        tm_time_tar.tm_mon = target_time.month;
        tm_time_tar.tm_mday = target_time.day;
        tm_time_tar.tm_hour = target_time.hour;
        tm_time_tar.tm_min = target_time.min;

        //Convert the tm struct into unix time stamps (local time)
        time_t current_time_unix = mktime(&tm_time_cur);
        time_t target_time_unix = mktime(&tm_time_tar);

        //compute the sleeping time into milliseconds
        uint sleep_timer = (target_time_unix - current_time_unix) * 1000;
        //convert the sleeping time into an absolute time point
        absolute_time_t target_abs_time = make_timeout_time_us(sleep_timer);
        //sleep until the target time has been reached
        sleep_until(target_abs_time);

    } else {
        DEBUG("The clock isn't synced");
    }
}