#include "PicoUart.hpp"
#include "gps.hpp"
#include "compass.hpp"
#include "stepper-motor.hpp"
#include "pico/stdio.h"
#include "pico/stdlib.h"
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
    std::vector<uint> optoforks(0);

    PIO pio = pio0;
    StepperMotor motor(stepperPins, optoforks);
    // StepperMotor motor2(stepperPins2, optoforks);
    
    motor.init(pio, 2, true);
    // motor2.init(pio1, 5, true);

    motor.turnSteps(10000);
    motor.stop();
    // motor2.turnSteps(10000);
    while (true) {
        for (auto pin : stepperPins) {
            std::cout << gpio_get(pin) << ", ";
        }
        std::cout << std::endl;
        motor.turnSteps(1);
    }

    return 0;
}
