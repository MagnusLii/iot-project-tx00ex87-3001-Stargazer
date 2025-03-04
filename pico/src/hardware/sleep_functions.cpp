#include "sleep_functions.hpp"
#include "pico/sleep.h"

#include "debug.hpp"
#include <hardware/regs/clocks.h>

static void alarm_sleep_callback(uint alarm_id) {
    DEBUG("Woken by timer \n");
    uart_default_tx_wait_blocking();
    hardware_alarm_set_callback(alarm_id, NULL);
    hardware_alarm_unclaim(alarm_id);
}

// remember to call sleep_power_up() after sleeping to enable the clocks and generators
bool sleep() {

    // Turn off all clocks except the RTC ones (I hope)
    clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_RTC_BITS;
    // Turn off all the clocks except the ones relating to UART, USB and Timer (I hope)
    clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS | CLOCKS_SLEEP_EN1_CLK_SYS_USBCTRL_BITS |
                               CLOCKS_SLEEP_EN1_CLK_USB_USBCTRL_BITS | CLOCKS_SLEEP_EN1_CLK_SYS_UART0_BITS |
                               CLOCKS_SLEEP_EN1_CLK_PERI_UART0_BITS | CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS |
                               CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS;

    stdio_flush();

    // Enable deep sleep at the proc
    scb_hw->scr |= ARM_CPU_PREFIXED(SCR_SLEEPDEEP_BITS);

    // Go to sleep
    __wfi();
    return true;
}
