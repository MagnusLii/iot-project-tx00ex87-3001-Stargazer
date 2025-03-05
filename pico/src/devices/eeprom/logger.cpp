#include "logger.hpp"

#define BAUD_RATE 1000000
#define WRITE_CYCLE_MAX_MS 10
#define EEPROM_SIZE 32768
#define HEAD_ADDR 0
#define TAIL_ADDR 2
#define START_ADDR 64

#include "debug.hpp"

logger::logger(i2c_inst_t* i2c, uint sda_pin, uint scl_pin) {
    this->i2c = i2c;
    eeprom_init_i2c(i2c, sda_pin, scl_pin, BAUD_RATE, WRITE_CYCLE_MAX_MS);

    eeprom_read_page(i2c,HEAD_ADDR, (uint8_t*)&head, sizeof(head));
    sleep_ms(10);
    eeprom_read_page(i2c,TAIL_ADDR, (uint8_t*)&tail, sizeof(tail));
    sleep_ms(10);

    if (head == 0xFFFF || tail == 0xFFFF || head >= EEPROM_SIZE || tail >= EEPROM_SIZE) {
        head = START_ADDR;
        tail = START_ADDR;
        eeprom_write_page(i2c, HEAD_ADDR, (uint8_t*)&head, sizeof(head));
        sleep_ms(10);
        eeprom_write_page(i2c, TAIL_ADDR, (uint8_t*)&tail, sizeof(tail));
        sleep_ms(10);
    }
}

bool logger::write(Command& command) {
    uint8_t buffer[sizeof(Command) + 2];

    memcpy(buffer, &command, sizeof(Command));

    uint16_t crc = crc16(buffer, sizeof(Command));
    buffer[sizeof(Command)] = crc >> 8;
    buffer[sizeof(Command) + 1] = crc & 0xFF;

    size_t total_size = sizeof(buffer);

    uint16_t new_head = head + total_size;
    if (new_head % 64 != 0) {
        new_head = ((new_head / 64) + 1) * 64;
    }

    if (new_head >= EEPROM_SIZE) {
        new_head = START_ADDR;
    }

    if (new_head == tail) {
        return false;
    }

    if (!write_page(head, buffer, total_size)) {
        return false;
    }

    head = new_head;
    eeprom_write_page(i2c, HEAD_ADDR, (uint8_t*)&head, sizeof(head));
    sleep_ms(10);

    return true;
}

bool logger::read(Command& command) {
    if (tail == head) {
        return false;
    }

    uint8_t buffer[sizeof(Command) + 2];

    eeprom_read_page(i2c, tail, buffer, sizeof(buffer));
    sleep_ms(10);

    uint16_t stored_crc = (buffer[sizeof(Command)] << 8) | buffer[sizeof(Command) + 1];
    uint16_t other_crc = crc16(buffer, sizeof(Command));

    if (stored_crc != other_crc) {
        return false;
    }

    memcpy(&command, buffer, sizeof(Command));

    tail += sizeof(buffer);

    tail = ((tail / 64) + 1) * 64;

    if (tail >= EEPROM_SIZE) {
        tail = START_ADDR;
    }

    eeprom_write_page(i2c, TAIL_ADDR, (uint8_t*)&tail, sizeof(tail));
    sleep_ms(10);
    return true;
}

void logger::clear_eeprom() {
    uint8_t empty_data[64] = {0};

    for (uint16_t addr = START_ADDR; addr < EEPROM_SIZE; addr += 64) {
        if (!write_page(addr, empty_data, sizeof(empty_data))) {
            DEBUG("Failed to clear EEPROM at address " + std::to_string(addr));
            sleep_ms(10);
            return;
        }
    }

    head = START_ADDR;
    tail = START_ADDR;
    eeprom_write_page(i2c, HEAD_ADDR, (uint8_t*)&head, sizeof(head));
    sleep_ms(10);
    eeprom_write_page(i2c, TAIL_ADDR, (uint8_t*)&tail, sizeof(tail));
    sleep_ms(10);
}

bool logger::write_page(uint16_t address, const uint8_t *data, size_t size) {
    uint8_t buffer[size + 2];
    buffer[0] = address >> 8;
    buffer[1] = address & 0xFF;
    memcpy(buffer + 2, data, size);
    int result = i2c_write_timeout_us(i2c, 0x50, buffer, size + 2, false, 1000);
//    i2c_write_blocking(i2c, 0x50, buffer, size + 2, false);
    sleep_ms(10);
    return (result == size + 2);
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