#include "compass.hpp"
#include "pico/stdlib.h"

#include "debug.hpp"

// Compass I2C configuration
#define COMPASS_ADDR  0x1E
#define CONFIG_A      0x00
#define CONFIG_B      0x01
#define MODE_REG      0x02
#define DATA_REG      0x03

// Conversion from raw value to microteslas (uT)
#define TO_UT         (100.0 / 1090.0)

Compass::Compass(uint SCL_PIN_VAL, uint SDL_PIN_VAL, i2c_inst_t* I2C_PORT_VAL)
    : SCL_PIN(SCL_PIN_VAL), SDA_PIN(SDL_PIN_VAL), I2C_PORT(I2C_PORT_VAL) {
    // Initialize I2C communication
    i2c_init(I2C_PORT, 400000); // 400 kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
}

// Initialize the compass
void Compass::init() {
    uint8_t config_a[2] = {CONFIG_A, 0x70}; // Configuration for CONFIG_A
    uint8_t config_b[2] = {CONFIG_B, 0xa0}; // Configuration for CONFIG_B

    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, config_a, 2, true);
    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, config_b, 2, true);

    calibrate();
}

// Read raw data from the compass
void Compass::readRawData(int16_t &x, int16_t &y, int16_t &z) {
    uint8_t buf[2] = {MODE_REG, CONFIG_B};
    uint8_t data[6];

    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, buf, 2, false);
    sleep_ms(10);

    // Request data
    buf[0] = DATA_REG;
    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, buf, 1, false);
    i2c_read_blocking(I2C_PORT, COMPASS_ADDR, data, 6, false);

    // Combine high and low bytes
    x = ((int16_t)data[0] << 8) | data[1];
    z = ((int16_t)data[2] << 8) | data[3];
    y = ((int16_t)data[4] << 8) | data[5];
}

void Compass::calibrate() {
    CalibrationMaxValue maxValue = {0, 0, 0};
    CalibrationMinValue minValue = {0, 0, 0};

    int xCount = 0;
    int yCount = 0;
    int zCount = 0;
    bool xRotationFlag = false;
    bool yRotationFlag = false;
    bool zRotationFlag = false;

    int16_t x, y, z;

    DEBUG("Calibrate the compass\r\n");

    while(xCount < 3 || yCount < 3 || zCount < 3) {
        readRawData(x, y, z);
        if ((fabs(x) > 600) || (fabs(y) > 600) || (fabs(z) > 600))
            continue;

        if (minValue.X > x) {
            minValue.X = x;
        } else if (maxValue.X < x) {
            maxValue.X = x;
        }

        if (minValue.Y > y) {
            minValue.Y = y;
        } else if (maxValue.Y < y)
        {
            maxValue.Y = y;
        }

        if (minValue.Z > z) {
            minValue.Z = z;
        } else if (maxValue.Z < z) {
            maxValue.Z = z;
        }

        if (xRotationFlag) {
            if (fabs(x) > 50) {
                xRotationFlag = false;
                xCount++;
            }
        } else {
            if (fabs(x) < 40) {
                xRotationFlag = true;
            }
        }

        if (yRotationFlag) {
            if (fabs(y) > 50) {
                yRotationFlag = false;
                yCount++;
            }
        } else {
            if (fabs(y) < 40) {
                yRotationFlag = true;
            }
        }

        if (zRotationFlag) {
            if (fabs(z) > 50) {
                zRotationFlag = false;
                zCount++;
            }
        } else {
            if (fabs(z) < 40)
            {
                zRotationFlag = true;
            }
        }

        sleep_ms(30);
    }

    xRawValueOffset = (maxValue.X + minValue.X) / 2;
    yRawValueOffset = (maxValue.Y + minValue.Y) / 2;
    zRawValueOffset = (maxValue.Z + minValue.Z) / 2;

    DEBUG("Calibration done");
}

// Calculate heading in degrees
float Compass::getHeading() {
    int16_t x, y, z;
    readRawData(x, y, z);

    x -= xRawValueOffset;
    y -= yRawValueOffset;
    z -= zRawValueOffset;

    // Convert raw values to microtesla
    float x_uT = x * TO_UT;
    float y_uT = y * TO_UT;
    float z_uT = z * TO_UT;

    // Calculate heading
    float heading = atan2(y_uT, x_uT);

    //This is the declination angle in radians, converted from +10* 16'. It is taken around Helsinki / Vantaa.
    float declinationAngle = 0.18;
    heading += declinationAngle;

    if (heading < 0)
        heading += 2*M_PI;

    if (heading > 2*M_PI) {
        heading -= 2*M_PI;
    }

    //convert radians to degrees
    float headingDegrees = heading * 180/M_PI;

    return headingDegrees;
}