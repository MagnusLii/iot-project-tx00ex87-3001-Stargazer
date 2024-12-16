#pragma once

#include "pico/stdlib.h"
#include <cstdlib>

class Motor {
  public:
    Motor(uint Pin_A, uint Pin_B, uint Pin_C, uint Pin_D, uint stepsPerRevolution = 4096, uint delayMs = 2);
    void initialize();
    void turnSteps(int steps, bool reverse = false);

  private:
    uint Pin_A, Pin_B, Pin_C, Pin_D;   // GPIO pins
    uint stepsPerRevolution;          // Steps per full revolution
    uint delayMs;                     // Delay between steps (in ms)
    uint8_t currentStep;              // Current step index
    const uint8_t stepSequence[8][4] = {
        {1, 0, 0, 0},
        {1, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 1},
        {0, 0, 0, 1},
        {1, 0, 0, 1}
    };

    void motorStep(uint8_t stepIndex);
    uint8_t adjustStep(uint8_t currentStep, bool reverse);
};