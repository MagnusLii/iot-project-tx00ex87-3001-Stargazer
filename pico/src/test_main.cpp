#include "pico/stdlib.h" // IWYU pragma: keep

#include "debug.hpp"
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
    sleep_ms(500);
    DEBUG("Boot");

    Celestial moon(SATURN);
    datetime_t date;
    date.year = 2025;
    date.month = 2;
    date.day = 23;
    date.hour = 16;
    date.min = 20;
    Coordinates coords(60.1699, 24.9384);
    moon.set_observer_coordinates(coords);
    azimuthal_coordinates abc = moon.get_coordinates(date);
    std::cout << "alt " << abc.altitude * 180 / M_PI << " azi " << abc.azimuth * 180 / M_PI << std::endl;
    moon.start_trace(date, 24);

    mh->turnSteps(500);
    mv->turnSteps(500);
    while (mh->isRunning()) ;
    while (mv->isRunning()) ;

    mctrl.calibrate();
    while (mctrl.isCalibrating()) ;

    Command command;
    command.time.year = 1000;
    while (true)  {
        command = moon.next_trace();
        if (command.time.year != -1) {
            mctrl.turn_to_coordinates(command.coords);
            std::cout <<"alt actual " << command.coords.altitude * 180 / M_PI << " azi actual " << command.coords.azimuth * 180 / M_PI << std::endl;
            while (mctrl.isRunning()) ;
        }

    }
    return 0;
}

