#pragma once

#include "hardware/clock.hpp"
#include "iostream"
#include "memory"
#include "hardware/rosc.h"
#include "hardware/clocks.h"
#include "pico/sleep.h"
#include "hardware/sync.h"
#include "pico/runtime_init.h"

bool sleep_for(uint hours, uint mins, hardware_alarm_callback_t callback);
void sleep_until_certain_time(const std::shared_ptr<Clock>& clock, const datetime_t target_time, hardware_alarm_callback_t callback);