#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include <vector>
#include <algorithm>

#define NPINS 4
#define RPM_MAX 15.
#define RPM_MIN 1.8

#define CLOCKWISE true
#define ANTICLOCKWISE false


class StepperMotor {
  friend void raw_calibration_handler(void);
  public:
    StepperMotor(const std::vector<uint> &stepper_pins, int optoforkpin=-1, bool horizontal=true);

    void init(PIO pio, float rpm, bool clockwise);

    void set_calibration_pointer(bool horizontal);
    void calibrate(void);

    void turnSteps(uint16_t steps);
    void turnOneRevolution();
    void setSpeed(float rpm);
    void stop();
    void setDirection(bool clockwise);

    uint8_t getCurrentStep() const;
    bool isRunning() const;
    bool isCalibrated() const;
    bool isCalibrating() const;
    uint16_t getMaxSteps() const;
    uint16_t getEdgeSteps() const;
    int16_t getStepCount() const;
    bool getDirection() const;

  private:
    void pioInit(void);
    float calculateClkDiv(float rpm) const;
    void morph_pio_pin_definitions(void);
    void pins_init();
    int read_steps_left(void);
    void calibration_handler(void);

    std::vector<uint> pins;    // Stepper motor pins
    int optoForkPin;          // Pin for opto fork sensor, for now unused
    bool direction;            // Motor direction: true for clockwise, false for anticlockwise
    PIO pioInstance;           // PIO instance
    uint programOffset;        // Program offset in PIO memory
    uint stateMachine;         // State machine index
    float speed;               // Current motor speed in RPM
    int8_t sequenceCounter;    // Sequence counter for step calculation
    int stepCounter;           // Total steps taken
    uint stepMax;              // Maximum number of steps for a full revolution
    uint edgeSteps;            // Steps at the edge of calibration
    uint64_t stepMemory;       // Tracks recent step movements
    bool stepperCalibrated;    // Whether the stepper is calibrated
    bool stepperCalibrating;   // Whether calibration is in progress
};

#endif // STEPPER_MOTOR_H
