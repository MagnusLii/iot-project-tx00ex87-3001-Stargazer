#pragma once

#include "pico/stdlib.h"

void datetime_increment_hour(datetime_t &date);
void datetime_increment_day(datetime_t &date);
bool is_leap_year(int year);