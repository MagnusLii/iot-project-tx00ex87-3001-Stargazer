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
    DEBUG("Booted\n");

    // uint stepperPins[] = {2, 3, 6, 13};
    std::vector<uint> stepperPins{2, 3, 6, 13};
    // std::vector<uint> stepperPins2{18, 19, 20, 21};
    uint optofork(0);

    PIO pio = pio0;
    // StepperMotor motor2(stepperPins2, optoforks);
    
    // motor2.init(pio1, 5, true);

    // motor2.turnSteps(10000);
    while (true) {
        for (auto pin : stepperPins) {
            std::cout << gpio_get(pin) << ", ";
        }
        std::cout << std::endl;
    }

    return 0;
}
