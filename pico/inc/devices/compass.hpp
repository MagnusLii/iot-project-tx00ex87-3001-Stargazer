#pragma once

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <cmath>
#include <cstdint>

#include "debug.hpp"

struct CalibrationMaxValue {
    float X;
    float Y;
    float Z;
};

struct CalibrationMinValue {
    float X;
    float Y;
    float Z;
};

class Compass {
  public:
    Compass(i2c_inst_t *I2C_PORT, uint SCL_PIN, uint SDA_PIN);
    void readRawData(int16_t &x, int16_t &y, int16_t &z);
    void calibrate();
    float getHeading();

  private:
    i2c_inst_t *I2C_PORT;
    uint SCL_PIN;
    uint SDA_PIN;
    float xRawValueOffset;
    float yRawValueOffset;
    float zRawValueOffset;
};
