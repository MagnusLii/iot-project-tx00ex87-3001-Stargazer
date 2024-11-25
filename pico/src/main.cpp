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
    sleep_ms(2000);
    auto gps = std::make_unique<GPS>(uart, GPS::StartType::NONE);
    sleep_ms(2000);
    gps->reset_system_defaults();
    gps->locate_position(10);
    gps->set_nmea_output_frequencies(0, 0, 0, 0, 0, 0);

    for (;;) {
        sleep_ms(1000);
        Coordinates coords = gps->get_coordinates();
        if (coords.status) {
            std::cout << "Lat: " << coords.latitude << " Lon: " << coords.longitude << std::endl;
            gps->set_mode(GPS::Mode::STANDBY);
        } else {
            std::cout << "No fix" << std::endl;
            gps->set_nmea_output_frequencies();
            gps->locate_position(20);
        }
    }

    return 0;
}
