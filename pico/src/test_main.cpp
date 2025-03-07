#include "pico/stdlib.h" // IWYU pragma: keep

#include "PicoUart.hpp"
#include "clock.hpp"
#include "commbridge.hpp"
#include "compass.hpp"
#include "controller.hpp"
#include "debug.hpp"
#include "gps.hpp"
#include "message.hpp"
#include "stepper-motor.hpp"
#include "motor-control.hpp"

#include <memory>
#include <queue>

#include "debug.hpp"
int main() {
    stdio_init_all();
    sleep_ms(500);
    
    // std::cout << "abc" << std::endl;
    // std::vector<uint> pins1{2, 3, 6, 13};
    // std::vector<uint> pins2{16,17,18,19};
    // int opto_horizontal = 14;
    // int opto_vertical = 28;
    // std::shared_ptr<StepperMotor> mh = std::make_shared<StepperMotor>(pins1);
    // std::shared_ptr<StepperMotor> mv = std::make_shared<StepperMotor>(pins2);
    // MotorControl mctrl(mh, mv, opto_horizontal, opto_vertical);
    // sleep_ms(500);
    // DEBUG("Boot");

    Celestial moon(MOON);
    datetime_t date;
    date.year = 2025;
    date.month = 3;
    date.day = 4;
    date.hour = 10;
    date.min = 14;
    Coordinates coords(60.22969, 24.99197);
    moon.set_observer_coordinates(coords);
    azimuthal_coordinates abc = moon.get_coordinates(date);
    std::cout << "alt " << abc.altitude * 180 / M_PI << " azi " << abc.azimuth * 180 / M_PI << std::endl;
    moon.start_trace(date, 24);


    
    while (1) {
        std::cout << "alt " << abc.altitude * 180 / M_PI << " azi " << abc.azimuth * 180 / M_PI << std::endl;
        sleep_ms(500);
    }
    return 0;
}

