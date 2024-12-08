#include "compass.hpp"

Compass::Compass() {
    // Initialize I2C communication
    i2c_init(I2C_PORT, 400000); // 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

// Initialize the compass
void Compass::init() {
    uint8_t config_a[2] = {CONFIG_A, 0x70}; // Configuration for CONFIG_A
    uint8_t config_b[2] = {CONFIG_B, 0xa0}; // Configuration for CONFIG_B

    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, config_a, 2, true);
    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, config_b, 2, true);
}

// Read raw data from the compass
void Compass::readRawData(int16_t &x, int16_t &y, int16_t &z) {
    uint8_t buf[1] = {DATA_REG};
    uint8_t data[6];

    // Request data
    i2c_write_blocking(I2C_PORT, COMPASS_ADDR, buf, 1, false);
    i2c_read_blocking(I2C_PORT, COMPASS_ADDR, data, 6, false);

    // Combine high and low bytes
    x = ((int16_t)data[0] << 8) | data[1];
    z = ((int16_t)data[2] << 8) | data[3];
    y = ((int16_t)data[4] << 8) | data[5];
}

// Calculate heading in degrees
float Compass::getHeading() {
    int16_t x, y, z;
    readRawData(x, y, z);

    // Convert raw values to microtesla
    float x_uT = x * TO_UT;
    float y_uT = y * TO_UT;

    // Calculate heading
    float heading = atan2(y_uT, x_uT) * (180.0 / M_PI);
    if (heading < 0) {
        heading += 360;
    }
    return heading;
}