#pragma once

#include <cmath>
#include <iostream>
#include <pico/stdlib.h>
#include <pico/util/datetime.h>



/*
PRIVATE
*/

float datetime_to_julian_day(datetime_t &date);