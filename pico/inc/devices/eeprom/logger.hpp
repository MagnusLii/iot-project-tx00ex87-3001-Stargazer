#pragma once
#include "eeprom.h"
#include "structs.hpp"
#include <memory>
#include <cstring>
#include <cstdint>

class logger {
  public:
    logger(i2c_inst_t* i2c, uint sda_pin, uint scl_pin);
    bool write(Command& command);
    bool read(Command& command);
    void clear_eeprom();
  private:
    bool write_page(uint16_t address, const uint8_t* data, size_t size);
    i2c_inst_t* i2c;
    uint16_t head = 0xFFFF;
    uint16_t tail = 0xFFFF;
};

uint16_t crc16(const uint8_t *data, size_t length);