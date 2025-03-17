#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "planet_finder.hpp"
#include <algorithm>
#include <cmath>
#include <stdint.h>
#include <vector>

#define NPINS   4
#define RPM_MAX 15.
#define RPM_MIN 1.8

#define CLOCKWISE     true
#define ANTICLOCKWISE false

int modulo(int x, int y);

class StepperMotor {
  public:
    StepperMotor(const std::vector<uint> &stepper_pins);

    void init(PIO pio, float rpm, bool clockwise);

    void turnSteps(uint16_t steps);
    void turn_to(double radians);
    void turnOneRevolution();
    void stop();
    void off();
    void setSpeed(float rpm);
    void setDirection(bool clockwise);
    void resetStepCounter(void);

    double get_position(void);
    uint8_t getCurrentStep() const;
    bool isRunning() const;
    uint16_t getMaxSteps() const;
    int16_t getStepCount() const;
    bool getDirection() const;

  private:
    void pioInit(void);
    float calculateClkDiv(float rpm) const;
    void morph_pio_pin_definitions(void);
    void pins_init();
    int read_steps_left(void);
    int radians_to_steps(double radians);

    std::vector<uint> pins; // Stepper motor pins
    bool direction;         // Motor direction: true for clockwise, false for anticlockwise
    PIO pioInstance;        // PIO instance
    uint programOffset;     // Program offset in PIO memory
    uint stateMachine;      // State machine index
    float speed;            // Current motor speed in RPM
    int8_t sequenceCounter; // Sequence counter for step calculation
    int stepCounter;        // Total steps taken
    uint stepMax;           // Maximum number of steps for a full revolution
    uint64_t stepMemory;    // Tracks recent step movements
};

#endif // STEPPER_MOTOR_H
