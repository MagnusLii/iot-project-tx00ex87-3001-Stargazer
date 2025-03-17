#include "compass.hpp"
#include "pico/stdlib.h"

#include "debug.hpp"

// Compass I2C configuration
#define COMPASS_ADDR 0x1E
#define CONFIG_A     0x00
#define CONFIG_B     0x01
#define MODE_REG     0x02
#define DATA_REG     0x03

// Conversion from raw value to microteslas (uT)
#define TO_UT (100.0 / 1090.0)

/**
 * @brief Constructor for Compass class, initializes I2C communication.
 *
 * Sets up I2C pins, configures pull-ups, and initializes compass registers.
 *
 * @param I2C_PORT_VAL Pointer to the I2C instance.
 * @param SCL_PIN_VAL GPIO pin number for SCL.
 * @param SDL_PIN_VAL GPIO pin number for SDA.
 */
Compass::Compass(i2c_inst_t *I2C_PORT_VAL, uint SCL_PIN_VAL, uint SDL_PIN_VAL)
    : I2C_PORT(I2C_PORT_VAL), SCL_PIN(SCL_PIN_VAL), SDA_PIN(SDL_PIN_VAL) {
    i2c_init(I2C_PORT, 400000); // 400 kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    xRawValueOffset = 0;
    yRawValueOffset = 0;
    zRawValueOffset = 0;

    uint8_t config_a[2] = {CONFIG_A, 0x70}; // Configuration for CONFIG_A
    uint8_t config_b[2] = {CONFIG_B, 0xa0}; // Configuration for CONFIG_B

    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, config_a, 2, false);
    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, config_b, 2, false);
}

/**
 * @brief Reads raw x, y, and z values from the compass sensor.
 *
 * Communicates with the compass over I2C to retrieve 6 bytes of data
 * representing the raw magnetic field strengths.
 *
 * @param x Reference to store the x-axis raw value.
 * @param y Reference to store the y-axis raw value.
 * @param z Reference to store the z-axis raw value.
 */
void Compass::readRawData(int16_t &x, int16_t &y, int16_t &z) {
    uint8_t buf[2] = {MODE_REG, CONFIG_B};
    uint8_t data[6];
    int ret = i2c_write_timeout_us(I2C_PORT, COMPASS_ADDR, buf, 2, false, 10000);
    if (ret == PICO_ERROR_TIMEOUT || ret == PICO_ERROR_GENERIC) {
        DEBUG("Can't read compass");
        return;
    }
    sleep_ms(10);

    // Request data
    buf[0] = DATA_REG;
    ret = i2c_write_timeout_us(I2C_PORT, COMPASS_ADDR, buf, 1, false, 10000);
    if (ret == PICO_ERROR_TIMEOUT || ret == PICO_ERROR_GENERIC) {
        DEBUG("Can't read compass");
        return;
    }
    sleep_ms(10);
    ret = i2c_read_timeout_us(I2C_PORT, COMPASS_ADDR, data, 6, false, 10000);
    if (ret == PICO_ERROR_TIMEOUT || ret == PICO_ERROR_GENERIC) {
        DEBUG("Can't read compass");
        return;
    }

    // Combine high and low bytes
    x = ((int16_t)data[0] << 8) | data[1];
    z = ((int16_t)data[2] << 8) | data[3];
    y = ((int16_t)data[4] << 8) | data[5];
}

/**
 * @brief Calibrates the compass by determining offsets for x, y, and z axes.
 *
 * The function repeatedly measures raw values and determines the min/max
 * values to calculate offsets, ensuring more accurate readings.
 */
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

    DEBUG("Calibrate the compass");

    while (xCount < 3 || yCount < 3 || zCount < 3) {
        readRawData(x, y, z);
        if ((std::fabs(x) > 600) || (std::fabs(y) > 600) || (std::fabs(z) > 600)) continue;

        if (minValue.X > x) minValue.X = x;
        else if (maxValue.X < x) maxValue.X = x;

        if (minValue.Y > y) minValue.Y = y;
        else if (maxValue.Y < y) maxValue.Y = y;

        if (minValue.Z > z) minValue.Z = z;
        else if (maxValue.Z < z) maxValue.Z = z;

        if (xRotationFlag) {
            if (std::fabs(x) > 50) { xRotationFlag = false; xCount++; }
        } else if (std::fabs(x) < 40) { xRotationFlag = true; }

        if (yRotationFlag) {
            if (std::fabs(y) > 50) { yRotationFlag = false; yCount++; }
        } else if (std::fabs(y) < 40) { yRotationFlag = true; }

        if (zRotationFlag) {
            if (std::fabs(z) > 50) { zRotationFlag = false; zCount++; }
        } else if (std::fabs(z) < 40) { zRotationFlag = true; }

        sleep_ms(30);
    }

    xRawValueOffset = (maxValue.X + minValue.X) / 2;
    yRawValueOffset = (maxValue.Y + minValue.Y) / 2;
    zRawValueOffset = (maxValue.Z + minValue.Z) / 2;

    DEBUG("Calibration done");
}

/**
 * @brief Computes and returns the compass heading in degrees.
 *
 * Uses the raw x and y values to determine the direction the compass is pointing,
 * factoring in a declination angle specific to the location.
 *
 * @return float The heading in degrees (0-360).
 */
float Compass::getHeading() {
    int16_t x, y, z;
    readRawData(x, y, z);

    x -= xRawValueOffset;
    y -= yRawValueOffset;
    z -= zRawValueOffset;

    float x_uT = x * TO_UT;
    float y_uT = y * TO_UT;
<<<<<<< HEAD
=======
    // float z_uT = z * TO_UT;
>>>>>>> afb4c010d06bfe91755ea4270c7db0c1757cf6f7

    float heading = atan2(y_uT, x_uT);
    float declinationAngle = 0.18;
    heading += declinationAngle;

    if (heading < 0) heading += 2 * M_PI;
    if (heading > 2 * M_PI) heading -= 2 * M_PI;

    return heading * 180 / M_PI;
}
