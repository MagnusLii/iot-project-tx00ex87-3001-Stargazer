#pragma once
#include "eeprom.h"
#include "structs.hpp"
#include <memory>
#include <cstring>
#include <cstdint>

class Storage {
  public:
    Storage(i2c_inst_t* i2c, uint sda_pin, uint scl_pin);
    bool store_command(Command& command);
    bool get_command(Command& command, uint64_t id);
    bool delete_command(uint64_t id);
    void clear_eeprom();
  private:
    bool write(Command& command, uint addr);
    bool read(Command& command, uint addr);
    bool write_page(uint16_t address, const uint8_t* data, size_t size);
    i2c_inst_t* i2c;
//    uint16_t head = 0xFFFF;
//    uint16_t tail = 0xFFFF;
};

uint16_t crc16(const uint8_t *data, size_t length);