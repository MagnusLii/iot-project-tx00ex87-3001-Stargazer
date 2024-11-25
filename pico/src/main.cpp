#include "PicoUart.hpp"
#include "gps.hpp"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include <iostream>
#include <memory>

#include "debug.hpp"

int main() {
    stdio_init_all();
    sleep_ms(1000);
    DEBUG("Booted");

    auto uart = std::make_shared<PicoUart>(0, 0, 1, 9600);
    auto gps = std::make_unique<GPS>(uart);
    sleep_ms(2000);
    gps->set_mode(GPS::Mode::STANDBY);
    sleep_ms(2000);
    gps->locate_position(30);

    for (;;) {
        sleep_ms(1000);
        Coordinates coords = gps->get_coordinates();
        if (coords.status) {
            std::cout << "Lat: " << coords.latitude << " Lon: " << coords.longitude << std::endl;
            gps->set_mode(GPS::Mode::STANDBY);
        } else {
            std::cout << "No fix" << std::endl;
            gps->set_mode(GPS::Mode::FULL_ON);
            gps->locate_position(30);
        }
    }

    return 0;
}
