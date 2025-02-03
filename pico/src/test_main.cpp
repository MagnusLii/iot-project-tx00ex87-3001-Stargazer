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

#include "stepper-motor.hpp"

#include "debug.hpp"

// int main() {
//     stdio_init_all();
//     Celestial moon(MOON);
//     datetime_t date;
//     date.year = 2025;
//     date.month = 2;
//     date.day = 2;
//     date.hour = 20;
//     date.min = 0;
//     Coordinates coords(60.1699, 24.9384);
//     moon.set_observer_coordinates(coords);
//     datetime_t date2 = moon.get_interest_point_time(ZENITH, date);
//     moon.print_coordinates(date, 48);
//     while (true) {
//         std::cout << (int)date2.month << ", " << (int)date2.day << ", " << (int)date2.hour << std::endl;
//         sleep_ms(500);
//     }
// }

int main() {
    stdio_init_all();
    std::vector<uint> pins1 = {2,3,6,13};
    std::vector<uint> pins2{16,17,18,19};
    StepperMotor motor1(pins1, 0);
    StepperMotor motor2(pins2, 0);
    motor1.init(pio0, 2, true);
    motor2.init(pio1, 2, false);
    uint opto = 4;
    gpio_init(opto);
    gpio_pull_up(opto);
    bool aa = true;
    while (true) {
        // motor1.turnSteps(1000);
        motor1.turnSteps(6000);
        while (aa) {
            if (gpio_get(opto)) {
                aa = false;
                motor1.stop();
            }
        }
        // motor1.turnSteps(200);
        // motor1.stop();
        // motor2.stop();

    }
}