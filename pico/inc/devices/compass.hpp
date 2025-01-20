#pragma once

#include <cstdint>
#include <cmath>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "debug.hpp"

// Compass I2C configuration
#define COMPASS_ADDR  0x1E
#define CONFIG_A      0x00
#define CONFIG_B      0x01
#define MODE_REG      0x02
#define DATA_REG      0x03

// Conversion from raw value to microteslas (uT)
#define TO_UT         (100.0 / 1090.0)

// I2C Pins and Instance
#define I2C_SCL_PIN   17
#define I2C_SDA_PIN   16
#define I2C_PORT      i2c0

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
    Compass();
    void init();
    void readRawData(int16_t &x, int16_t &y, int16_t &z);
    void calibrate();
    float getHeading();
  private:
    float xRawValueOffset;
    float yRawValueOffset;
    float zRawValueOffset;
};