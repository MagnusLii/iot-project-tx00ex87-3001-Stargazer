#include "PicoUart.hpp"
#include "gps.hpp"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include <iostream>
#include <memory>

#include "debug.hpp"

int main() {
    stdio_init_all();
    sleep_ms(5000);
    DEBUG("Booted");

    auto uart = std::make_shared<PicoUart>(0, 0, 1, 9600);
    auto gps = std::make_unique<GPS>(uart);

    gps->locate_position(30);
    for (;;) {
        sleep_ms(1000);
        Coordinates coords = gps->get_coordinates();
        if (coords.status) {
            std::cout << "Lat: " << coords.latitude << " Lon: " << coords.longitude << std::endl;
        } else {
            std::cout << "No fix" << std::endl;
        }
    }

    return 0;
}
