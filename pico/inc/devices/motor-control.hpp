#pragma once

#include "stepper-motor.hpp"
#include "pico/stdlib.h"
#include <stdint.h>
#include <cmath>

#define MAX_ANGLE M_PI // half of a full turn (180 degrees)
#define MIN_ANGLE 0

enum Axis {
    HORIZONTAL,
    VERTICAL
};

class MotorControl {
    friend void raw_calibration_handler(void);
    public:
      MotorControl(std::shared_ptr<StepperMotor> horizontal, std::shared_ptr<StepperMotor> vertical, int optopin_horizontal=-1, int optopin_vertical=-1);
      bool turn_to_coordinates(azimuthal_coordinates coords);
      void off(void);

      void calibrate(void);
      bool isCalibrated(void) const;
      bool isCalibrating(void) const;
      bool isRunning(void) const;
    private:
      void init_optoforks(void);
      void calibration_handler(Axis axis, bool rise);
      std::shared_ptr<StepperMotor> motor_horizontal;
      std::shared_ptr<StepperMotor> motor_vertical;
      int opto_horizontal;
      int opto_vertical;
      bool horizontal_calibrated;
      bool vertical_calibrated;
      bool horizontal_calibrating;
      bool vertical_calibrating;
      bool handler_attached;
  };