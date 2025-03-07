#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>
#include <stdbool.h>


#define EEPROM_ADDRESS 0x50

static uint64_t write_cycle_max = 0;
static absolute_time_t write_init_time;

/**
 * Checks if the EEPROM write cycle duration has exceeded the maximum allowed time.
 *
 * @return true if the EEPROM write cycle duration exceeds the maximum, otherwise false.
 */
static inline bool eeprom_write_cycle_check() {
    if (absolute_time_diff_us(write_init_time, get_absolute_time()) > write_cycle_max) {
        return true;
    }
    return false;
}

/**
 * Blocks the execution until the EEPROM write cycle duration falls below the maximum allowed time.
 * Sleeps until the EEPROM write cycle duration is within the allowed limit.
 */
static inline void eeprom_write_cycle_block() {
    if (!eeprom_write_cycle_check()) {
        sleep_until(delayed_by_us(write_init_time, write_cycle_max));
    }
}

/**
 * Initializes the EEPROM module using the specified I2C interface, baud rate, and maximum write cycle duration.
 *
 * @param i2c              Pointer to the I2C interface (i2c_inst_t) to be used for EEPROM communication.
 * @param baud             Baud rate for the I2C communication.
 * @param write_cycle_max_ms Maximum allowed duration for EEPROM write cycles in milliseconds.
 */
void eeprom_init_i2c(i2c_inst_t *i2c,uint sda_pin, uint scl_pin, uint baud, uint32_t write_cycle_max_ms) {

    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_set_dir(sda_pin, GPIO_OUT);
    gpio_set_dir(scl_pin, GPIO_OUT);

    i2c_init(i2c, baud);

    write_cycle_max = (uint64_t) write_cycle_max_ms * 1000;

    write_init_time = nil_time;
}

/**
 * Writes the specified EEPROM address using the I2C interface after ensuring the EEPROM write cycle duration is within limits.
 *
 * @param address The address to be written to the EEPROM.
 */
void eeprom_write_address(i2c_inst_t *i2c, uint16_t address) {
    eeprom_write_cycle_block();

    uint8_t out[2] = {address >> 8, address}; // shift by 8 not 4...
    i2c_write_timeout_us(i2c, EEPROM_ADDRESS, out, 2, true, 1000);
//    i2c_write_blocking(i2c0, EEPROM_ADDRESS, out, 2, true);
}

/**
 * Writes a byte of data to the specified EEPROM address using the I2C interface after ensuring the EEPROM write cycle duration is within limits.
 *
 * @param address The address in the EEPROM where the byte will be written.
 * @param c       The byte of data to be written.
 */
void eeprom_write_byte(uint16_t address, char c) {
    uint8_t out[3] = {address >> 8, address, c};

    eeprom_write_cycle_block();

    i2c_write_blocking(i2c0, EEPROM_ADDRESS, out, 3, false);

    write_init_time = get_absolute_time();
}

/**
 * Writes a page of data to the specified EEPROM address using the I2C interface after ensuring the EEPROM write cycle duration is within limits.
 *
 * @param address The starting address in the EEPROM where the page will be written.
 * @param src     Pointer to the source data to be written to the EEPROM.
 * @param size    Size of the data (page size) to be written.
 */
void eeprom_write_page(i2c_inst_t *i2c, uint16_t address, uint8_t *src, size_t size) {
    uint8_t out[size + 2];
    out[0] = address >> 8; // Upper bits of the address
    out[1] = address; // Lower bits of the address
    for (int i = 0; i < size; i++) {
        out[i + 2] = src[i];
    }

    eeprom_write_cycle_block(); // Ensure EEPROM write cycle duration is within limits
    i2c_write_timeout_us(i2c, EEPROM_ADDRESS, out, size + 2, false, 1000);
//    i2c_write_blocking(i2c, EEPROM_ADDRESS, out, size + 2, false);

    write_init_time = get_absolute_time();
}

/**
 * Reads a byte of data from the specified EEPROM address using the I2C interface.
 *
 * @param address The address in the EEPROM from where the byte will be read.
 * @return The byte of data read from the EEPROM.
 */
//char eeprom_read_byte(uint16_t address) {
//    char c = 0;
//
//    eeprom_write_address(address);
//
//    i2c_read_blocking(i2c0, EEPROM_ADDRESS, &c, 1, false);
//
//    return c;
//}

/**
 * Reads a page of data from the specified EEPROM address into the provided destination buffer using the I2C interface.
 *
 * @param i2c     The i2c instance
 * @param address The starting address in the EEPROM from where the page will be read.
 * @param dst     Pointer to the destination buffer to store the read data.
 * @param size    Size of the data (page size) to be read.
 */
void eeprom_read_page(i2c_inst_t *i2c, uint16_t address, uint8_t *dst, size_t size) {
    eeprom_write_address(i2c, address);

    i2c_read_timeout_us(i2c, EEPROM_ADDRESS, dst, size, false, 1000);
//    i2c_read_blocking(i2c, EEPROM_ADDRESS, dst, size, false);
}