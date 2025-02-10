#pragma once

#include "hardware/clock.hpp"
#include "iostream"
#include "memory"
#include "hardware/rosc.h"
#include "hardware/clocks.h"
#include "pico/sleep.h"
#include "hardware/sync.h"
#include "pico/runtime_init.h"

// 0x00200000 and 0x00400000 (rtc registers)
#define EN0_REGS CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS + CLOCKS_SLEEP_EN0_CLK_SYS_RTC_BITS
// 0x00000800 and 0x00000400 (usb registers)
#define USB_REGS CLOCKS_SLEEP_EN1_CLK_USB_USBCTRL_BITS + CLOCKS_SLEEP_EN1_CLK_SYS_USBCTRL_BITS
// 0x00000080, 0x00000040, 0x00000200 and 0x00000100 (UART registers)
#define UART_REGS CLOCKS_SLEEP_EN1_CLK_SYS_UART0_BITS + CLOCKS_SLEEP_EN1_CLK_PERI_UART0_BITS + CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS + CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS
// USB, UART and timer registers
#define EN1_REGS USB_REGS + UART_REGS + CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS

bool sleep_for(uint hours, uint mins);
void sleep_until_certain_time(const std::shared_ptr<Clock>& clock, const datetime_t target_time);