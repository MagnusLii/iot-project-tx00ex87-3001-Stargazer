#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"

#define NPINS 4
#define RPM_MAX 15.
#define RPM_MIN 1.8

class StepperMotor {
  public:
    StepperMotor();

    void init(PIO pio, const uint *stepperPins, uint optoForkPin, float rpm, bool clockwise);

    void turnSteps(uint16_t steps);
    void turnOneRevolution();
    void setSpeed(float rpm);
    void stop();
    void setDirection(bool clockwise);

    void calibrate();
    void halfCalibrate(uint16_t maxSteps, uint16_t edgeSteps, uint pillsDispensed);

    uint8_t getCurrentStep() const;
    int32_t readStepsLeft() const;
    bool isRunning() const;
    bool isCalibrated() const;
    bool isCalibrating() const;
    uint16_t getMaxSteps() const;
    uint16_t getEdgeSteps() const;
    int16_t getStepCount() const;
    bool getDirection() const;

  private:
    void pioInit(float div);
    float calculateClkDiv(float rpm) const;

    uint pins[NPINS];          // Stepper motor pins
    uint optoForkPin;          // Pin for opto fork sensor, for now unused
    bool direction;            // Motor direction: true for clockwise, false for anticlockwise
    PIO pioInstance;           // PIO instance
    uint programOffset;        // Program offset in PIO memory
    uint stateMachine;         // State machine index
    float speed;               // Current motor speed in RPM
    uint sequenceCounter;      // Sequence counter for step calculation
    uint stepCounter;          // Total steps taken
    uint stepMax;              // Maximum number of steps for a full revolution
    uint edgeSteps;            // Steps at the edge of calibration
    uint stepMemory;           // Tracks recent step movements
    bool stepperCalibrated;    // Whether the stepper is calibrated
    bool stepperCalibrating;   // Whether calibration is in progress
};

#endif // STEPPER_MOTOR_H
