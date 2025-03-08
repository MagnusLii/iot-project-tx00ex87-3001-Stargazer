#include "storage.hpp"

#define BAUD_RATE          1000000
#define WRITE_CYCLE_MAX_MS 10
#define EEPROM_SIZE        32768
#define START_ADDR         0

#include "debug.hpp"

Storage::Storage(i2c_inst_t *i2c, uint sda_pin, uint scl_pin) {
    this->i2c = i2c;
    eeprom_init_i2c(i2c, sda_pin, scl_pin, BAUD_RATE, WRITE_CYCLE_MAX_MS);
}

bool Storage::store_command(Command &command) {
    uint8_t buffer[sizeof(Command) + 3];
    int page = START_ADDR;
    for (int i = 0; i < 512; ++i) {
        eeprom_read_page(i2c, page, buffer, sizeof(buffer));
        sleep_ms(10);
        if (buffer[sizeof(Command)] != 1) { break; }
        page += 64;
        if (page == EEPROM_SIZE) { return false; }
    }
    write(command, page);
    return true;
}

bool Storage::get_command(Command &command, uint64_t id) {
    uint8_t buffer[sizeof(Command) + 3];
    int page = START_ADDR;
    for (int i = 0; i < 512; ++i) {
        eeprom_read_page(i2c, page, buffer, sizeof(buffer));
        sleep_ms(10);
        if (buffer[sizeof(Command)] == 1 && static_cast<uint64_t>(buffer[0]) == id) {
            uint16_t stored_crc = (buffer[sizeof(Command) + 1] << 8) | buffer[sizeof(Command) + 2];
            uint16_t other_crc = crc16(buffer, sizeof(Command));

            if (stored_crc != other_crc) {
                DEBUG("Checksum doesn't match");
                return false;
            }
            memcpy(&command, buffer, sizeof(Command));
            return true;
        }
        page += 64;
        if (page == EEPROM_SIZE) { return false; }
    }
    return false;
}

bool Storage::write(Command &command, uint addr) {
    uint8_t buffer[sizeof(Command) + 3];
    buffer[sizeof(Command)] = 1;
    memcpy(buffer, &command, sizeof(Command));

    uint16_t crc = crc16(buffer, sizeof(Command));
    buffer[sizeof(Command) + 1] = crc >> 8;
    buffer[sizeof(Command) + 2] = crc & 0xFF;

    size_t total_size = sizeof(buffer);

    if (!write_page(addr, buffer, total_size)) { return false; }

    return true;
}

bool Storage::read(Command &command, uint addr) {

    uint8_t buffer[sizeof(Command) + 3];

    eeprom_read_page(i2c, addr, buffer, sizeof(buffer));
    sleep_ms(10);

    uint16_t stored_crc = (buffer[sizeof(Command) + 1] << 8) | buffer[sizeof(Command) + 2];
    uint16_t other_crc = crc16(buffer, sizeof(Command));

    if (stored_crc != other_crc) {
        DEBUG("Checksum doesn't match");
        return false;
    }

    memcpy(&command, buffer, sizeof(Command));

    return true;
}

void Storage::clear_eeprom() {
    uint8_t empty_data[64] = {0};

    for (uint16_t addr = START_ADDR; addr < EEPROM_SIZE; addr += 64) {
        if (!write_page(addr, empty_data, sizeof(empty_data))) {
            DEBUG("Failed to clear EEPROM at address " + std::to_string(addr));
            sleep_ms(10);
            return;
        }
    }
}

bool Storage::delete_command(uint64_t id) {
    uint8_t buffer[sizeof(Command) + 3];
    uint8_t empty_buff[64] = {0};
    int page = START_ADDR;
    for (int i = 0; i < 512; ++i) {
        eeprom_read_page(i2c, page, buffer, sizeof(buffer));
        sleep_ms(10);
        if (buffer[sizeof(Command)] == 1 && static_cast<uint64_t>(buffer[0]) == id) {
            eeprom_write_page(i2c, page, empty_buff, sizeof(empty_buff));
            return true;
        }
        page += 64;
        if (page == EEPROM_SIZE) { return false; }
    }
    return false;
}

int Storage::get_all_commands(std::vector<Command> &vector) {
    int i = 0;
    uint8_t buffer[sizeof(Command) + 3];
    int page = START_ADDR;
    for (; i < 512; ++i) {
        Command command;
        eeprom_read_page(i2c, page, buffer, sizeof(buffer));
        sleep_ms(10);
        if (buffer[sizeof(Command)] == 1) {
            uint16_t stored_crc = (buffer[sizeof(Command) + 1] << 8) | buffer[sizeof(Command) + 2];
            uint16_t other_crc = crc16(buffer, sizeof(Command));

            if (stored_crc != other_crc) {
                DEBUG("Checksum doesn't match");
            } else {
                memcpy(&command, buffer, sizeof(Command));
                vector.push_back(command);
            }
        }
        page += 64;
    }
    return i;
}

bool Storage::write_page(uint16_t address, const uint8_t *data, size_t size) {
    uint8_t buffer[size + 2];
    buffer[0] = address >> 8;
    buffer[1] = address & 0xFF;
    memcpy(buffer + 2, data, size);
    int result = i2c_write_timeout_us(i2c, 0x50, buffer, size + 2, false, 1000);
    //    i2c_write_blocking(i2c, 0x50, buffer, size + 2, false);
    sleep_ms(10);
    return (result == static_cast<int>((size + 2u)));
}

uint16_t crc16(const uint8_t *data, size_t length) {
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--) {
        x = crc >> 8 ^ *data++;
        x ^= x >> 4;
        crc =
            (crc << 8) ^ (static_cast<uint16_t>(x << 12)) ^ (static_cast<uint16_t>(x << 5)) ^ static_cast<uint16_t>(x);
    }
    return crc;
}