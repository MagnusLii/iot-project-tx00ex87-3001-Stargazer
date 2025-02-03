#pragma once

#include "hardware/clock.hpp"
#include "iostream"
#include "memory"
//include "pico/sleep.h"

void sleep_for(uint hours, uint mins);
void sleep_until_certain_time(const std::shared_ptr<Clock>& clock, const datetime_t target_time);