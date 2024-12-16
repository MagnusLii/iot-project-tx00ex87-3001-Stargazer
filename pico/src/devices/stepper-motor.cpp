#include "stepper-motor.hpp"

Motor::Motor(uint Pin_A, uint Pin_B, uint Pin_C, uint Pin_D, uint stepsPerRevolution, uint delayMs)
    : Pin_A(Pin_A), Pin_B(Pin_B), Pin_C(Pin_C), Pin_D(Pin_D),
      stepsPerRevolution(stepsPerRevolution), delayMs(delayMs), currentStep(0) {}

void Motor::initialize() {
    gpio_init(Pin_A);
    gpio_init(Pin_B);
    gpio_init(Pin_C);
    gpio_init(Pin_D);

    gpio_set_dir(Pin_A, GPIO_OUT);
    gpio_set_dir(Pin_B, GPIO_OUT);
    gpio_set_dir(Pin_C, GPIO_OUT);
    gpio_set_dir(Pin_D, GPIO_OUT);
}

void Motor::motorStep(uint8_t stepIndex) {
    gpio_put(Pin_A, stepSequence[stepIndex][0]);
    gpio_put(Pin_B, stepSequence[stepIndex][1]);
    gpio_put(Pin_C, stepSequence[stepIndex][2]);
    gpio_put(Pin_D, stepSequence[stepIndex][3]);
}

uint8_t Motor::adjustStep(uint8_t currentStep, bool reverse) {
    if (reverse) {
        return (currentStep == 0) ? 7 : currentStep - 1;
    }
    return (currentStep + 1) % 8;
}

void Motor::turnSteps(int steps, bool reverse) {
    for (int i = 0; i < abs(steps); i++) {
        currentStep = adjustStep(currentStep, reverse);
        motorStep(currentStep);
        sleep_ms(delayMs);
    }
}