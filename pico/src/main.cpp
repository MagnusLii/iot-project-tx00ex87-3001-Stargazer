#include "PicoUart.hpp"
#include "clock.hpp"
#include "compass.hpp"
#include "gps.hpp"
#include "compass.hpp"
#include "stepper-motor.hpp"
#include "compass.hpp"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include <hardware/rtc.h>
#include <iostream>
#include <memory>
#include <vector>

#include "debug.hpp"

int main() {
    stdio_init_all();
    sleep_ms(2000);
    DEBUG("Booted");

    // uint stepperPins[] = {2, 3, 6, 13};
    std::vector<uint> stepperPins{2, 3, 6, 13};
    std::vector<uint> optoforks(0);

    PIO pio = pio0;
    StepperMotor motor(stepperPins, optoforks);
    motor.init(pio, 1, true);

    motor.turnSteps(1000);
    while (true) {
        for (auto pin : stepperPins) {
            std::cout << gpio_get(pin) << ", ";
        }
        std::cout << std::endl;
    }
    /* Compass compass;
    compass.init();

    float heading;

    auto uart = std::make_shared<PicoUart>(0, 0, 1, 9600);
    auto gps = std::make_unique<GPS>(uart);
    auto clock = std::make_shared<Clock>();
    sleep_ms(2000);

    while (!clock->is_synced()) {
        DEBUG("Waiting for RTC to sync");
        // Request time from ESP32
        sleep_ms(5000);
        uint64_t unixtime_via_esp32 = 1733785923;
        clock->update(unixtime_via_esp32);
    }

    sleep_ms(10000);
    datetime_t now = clock->get_datetime();
    DEBUG("Time:", now.year, "-", unsigned(now.month), "-", unsigned(now.day), " ", unsigned(now.hour), ":",
          unsigned(now.min), ":", unsigned(now.sec));

    heading = compass.getHeading();
    printf("we are pointing at %.2f degrees", heading);

    gps->locate_position(600);
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
*/

    return 0;
}
