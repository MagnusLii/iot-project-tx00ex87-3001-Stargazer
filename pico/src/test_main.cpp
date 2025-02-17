#include "commbridge.hpp"
#include "convert.hpp"
#include "devices/gps.hpp"
#include "hardware/clock.hpp"
#include "devices/compass.hpp"
#include "message.hpp"
#include "pico/stdlib.h" // IWYU pragma: keep
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
#include "motor-control.hpp"

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
    std::cout << "abc" << std::endl;
    std::vector<uint> pins1{2, 3, 6, 13};
    std::vector<uint> pins2{16,17,18,19};
    int opto_horizontal = 14;
    int opto_vertical = 28;
    std::shared_ptr<StepperMotor> mh = std::make_shared<StepperMotor>(pins1);
    std::shared_ptr<StepperMotor> mv = std::make_shared<StepperMotor>(pins2);
    MotorControl mctrl(mh, mv, opto_horizontal, opto_vertical);

    Celestial moon(MOON);
    datetime_t date;
    date.year = 2025;
    date.month = 2;
    date.day = 8;
    date.hour = 14;
    date.min = 0;
    Coordinates coords(60.1699, 24.9384);
    moon.set_observer_coordinates(coords);
    moon.start_trace(date, 24);

    mh->turnSteps(500);
    mv->turnSteps(500);
    while (mh->isRunning()) ;
    while (mv->isRunning()) ;

    mctrl.calibrate();
    while (mctrl.isCalibrating()) ;

    while (true) ;

    // Command command;
    // command.time.year = 1000;
    // while (true)  {
    //     command = moon.next_trace();
    //     if (command.time.year != -1) {
    //         mv.turn_to(command.coords.altitude);
    //         mh.turn_to(command.coords.azimuth);
    //         std::cout <<"alt " << command.coords.altitude * 180 / M_PI << " azi " << command.coords.azimuth * 180 / M_PI << std::endl;
    //         while (mv.isRunning()) ;
    //         while (mh.isRunning()) ;
    //     }

    // }
}

