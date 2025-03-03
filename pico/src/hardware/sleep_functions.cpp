#include "sleep_functions.hpp"
#include "pico/sleep.h"

#include "debug.hpp"
#include <hardware/regs/clocks.h>

// remember to call sleep_power_up() after sleeping to enable the clocks and generators
bool sleep() {

    // Turn off all clocks except the RTC ones (I hope)
    clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_RTC_BITS;
    // Turn off all the clocks except the ones relating to UART, USB and Timer (I hope)
    clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_RTC_BITS;

    stdio_flush();

    // Enable deep sleep at the proc
    scb_hw->scr |= ARM_CPU_PREFIXED(SCR_SLEEPDEEP_BITS);

    // Go to sleep
    __wfi();
    return true;
}
