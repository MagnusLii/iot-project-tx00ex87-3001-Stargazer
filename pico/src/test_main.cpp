#include "commbridge.hpp"
#include "convert.hpp"
#include "devices/gps.hpp"
#include "hardware/clock.hpp"
#include "devices/compass.hpp"
#include "message.hpp"
#include "planet_finder.hpp"
#include "pico/stdlib.h"
#include "uart/PicoUart.hpp"
#include <hardware/timer.h>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <vector>

#include "debug.hpp"

int main() {
    stdio_init_all();
    Celestial moon(MOON);
    datetime_t date;
    date.year = 2025;
    date.month = 1;
    date.day = 28;
    date.hour = 10;
    Coordinates coords(60.1699, 24.9384);
    moon.fill_coordinate_table(date, coords);
    datetime_t date2 = moon.get_interest_point_time(ZENITH);
    moon.print_coordinate_table();
    while (true) {
        
    }
}
