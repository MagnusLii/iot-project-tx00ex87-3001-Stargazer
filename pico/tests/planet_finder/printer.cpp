#include "planet_finder.hpp"
#include <string>
#include <iostream>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    Celestial moon(URANUS);
    datetime_t date;
    date.year = 2025;
    date.month = 1;
    date.day = 28;
    date.hour = 10;
    Coordinates coords(60.1699, 24.9384);

    while (true) ;
}