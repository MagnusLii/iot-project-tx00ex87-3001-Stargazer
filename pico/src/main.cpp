#include "PicoUart.hpp"
#include "gps.hpp"
#include "compass.hpp"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include <iostream>
#include <memory>

#include "debug.hpp"

int main() {
    stdio_init_all();
    sleep_ms(5000);
    DEBUG("Booted");

    Compass compass;
    compass.init();

    float heading;

    auto uart = std::make_shared<PicoUart>(0, 0, 1, 9600);
    auto gps = std::make_unique<GPS>(uart);

    heading = compass.getHeading();
    printf("we are pointing at %.2f degrees", heading);

    gps->locate_position(600);
    for (;;) {
        sleep_ms(1000);
        Coordinates coords = gps->get_coordinates();
        if (coords.status) {
            std::cout << "Lat: " << coords.latitude << " Lon: " << coords.longitude << std::endl;
        } else {
            std::cout << "No fix" << std::endl;
            gps->locate_position(600);
        }
    }

    return 0;
}
