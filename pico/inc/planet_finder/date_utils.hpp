#pragma once

#include "pico/stdlib.h"
#include "convert.hpp"

void datetime_increment_hour(datetime_t &date);
void datetime_add_hours(datetime_t &date, uint hours);
void datetime_increment_day(datetime_t &date);
void datetime_increment_year(datetime_t &date);
bool is_leap_year(int year);
int calculate_hour_difference(const datetime_t& dt1, const datetime_t& dt2);
int calculate_sec_difference(const datetime_t &dt1, const datetime_t &dt2);

int64_t datetime_to_epoch(datetime_t date);
