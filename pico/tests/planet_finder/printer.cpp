#include "planet_finder.hpp"
#include <string>

int main() {
    stdio_init_all();
    Celestial moon(URANUS);
    datetime_t date;
    date.year = 2025;
    date.month = 1;
    date.day = 13;
    date.hour = 10;
    Coordinates coords(60.1699, 24.9384);
    moon.fill_coordinate_table(date, coords);
    moon.print_coordinate_table();

    while (true) ;
}