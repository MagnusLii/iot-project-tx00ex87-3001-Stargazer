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
    //gps->locate_position(15);
    //sleep_ms(2000);
    //gps->set_mode(GPS::Mode::ALWAYSLOCATE);

    for (;;) {
        sleep_ms(1000);
        Coordinates coords = gps->get_coordinates();
        if (coords.status) {
            gps->set_mode(GPS::Mode::STANDBY);
            for (;;) {
                std::cout << "Lat: " << coords.latitude << " Lon: " << coords.longitude << std::endl;
                sleep_ms(10000);
            }
        } else {
            gps->set_mode(GPS::Mode::STANDBY);
            std::cout << "No fix, trying again in 5s" << std::endl;
            sleep_ms(5000);
            gps->set_mode(GPS::Mode::FULL_ON);
            gps->locate_position(300);
        }
    }

    return 0;
}
