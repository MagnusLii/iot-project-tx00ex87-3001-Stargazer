#include "planet_finder.hpp"
#include <string>

int main() {
    stdio_init_all();
    Celestial moon(MOON);
    datetime_t date;
    date.year = 2025;
    date.month = 1;
    date.day = 5;
    Coordinates coords(60.1699, 24.9384);
    moon.fill_coordinate_table(date, coords);
    moon.print_coordinate_table();

    while (true) ;
}