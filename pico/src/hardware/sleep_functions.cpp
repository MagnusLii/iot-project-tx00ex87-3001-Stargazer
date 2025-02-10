#include "sleep_functions.hpp"
#include "pico/sleep.h"

#include "debug.hpp"
/*
static bool awake;

static void alarm_sleep_callback(uint alarm_id) {
    printf("alarm woke us up\n");
    uart_default_tx_wait_blocking();
    awake = true;
    hardware_alarm_set_callback(alarm_id, NULL);
    hardware_alarm_unclaim(alarm_id);
}

//cant use this from the sleep.h file, bring it here then
static void processor_deep_sleep(void) {
    // Enable deep sleep at the proc
#ifdef __riscv
    uint32_t bits = RVCSR_MSLEEP_POWERDOWN_BITS;
    if (!get_core_num()) {
        bits |= RVCSR_MSLEEP_DEEPSLEEP_BITS;
    }
    riscv_set_csr(RVCSR_MSLEEP_OFFSET, bits);
#else
    scb_hw->scr |= ARM_CPU_PREFIXED(SCR_SLEEPDEEP_BITS);
#endif
}

// remember to call sleep_power_up() after sleeping to enable the clocks and generators
bool sleep_for(uint hours, uint mins) {

    if (mins > 60) {
        while (mins >= 60) {
            hours++;
            mins -= 60;
        }
    }

    uint hours_ms = hours * 3.6e6;  //convert hours into milliseconds
    uint mins_ms = mins * 6e4;      //Convert minutes into milliseconds

    uint total_time = hours_ms + mins_ms;

    awake = false;

    //Turn off all clocks except the RTC ones (I hope)
    clocks_hw->sleep_en0 = 0x0;
    //Turn off all the clocks except the ones relating to UART and USB (I hope)
    clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS;

    int alarm_num = hardware_alarm_claim_unused(true);
    hardware_alarm_set_callback(alarm_num, alarm_sleep_callback);
    absolute_time_t t = make_timeout_time_ms(10000);
    if (hardware_alarm_set_target(alarm_num, t)) {
        hardware_alarm_set_callback(alarm_num, NULL);
        hardware_alarm_unclaim(alarm_num);
        return false;
    }

    stdio_flush();

    // Enable deep sleep at the processor
    processor_deep_sleep();

    // Go to sleep
    __wfi();
}


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

        //very likely to be redundant
        //convert the sleeping time into an absolute time point
        //absolute_time_t target_abs_time = make_timeout_time_us(sleep_timer);

        awake = false;

        //Turn off all clocks except the RTC ones (I hope)
        clocks_hw->sleep_en0 = EN0_REGS;
        //Turn off all the clocks except the ones relating to UART and USB (I hope)
        clocks_hw->sleep_en1 = EN1_REGS;

        int alarm_num = hardware_alarm_claim_unused(true);
        hardware_alarm_set_callback(alarm_num, &alarm_sleep_callback);
        absolute_time_t t = make_timeout_time_ms(sleep_timer);
        if (hardware_alarm_set_target(alarm_num, t)) {
            hardware_alarm_set_callback(alarm_num, NULL);
            hardware_alarm_unclaim(alarm_num);
        }

        stdio_flush();

        // Enable deep sleep at the processor
        processor_deep_sleep();

        // Go to sleep
        __wfi();

    } else {
        DEBUG("The clock isn't synced");
    }

}
*/