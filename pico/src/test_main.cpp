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
    date.month = 2;
    date.day = 2;
    date.hour = 20;
    date.min = 0;
    Coordinates coords(60.1699, 24.9384);
    moon.set_observer_coordinates(coords);
    datetime_t date2 = moon.get_interest_point_time(ZENITH, date);
    moon.print_coordinates(date, 48);
    while (true) {
        std::cout << (int)date2.month << ", " << (int)date2.day << ", " << (int)date2.hour << std::endl;
        sleep_ms(500);
    }
}
