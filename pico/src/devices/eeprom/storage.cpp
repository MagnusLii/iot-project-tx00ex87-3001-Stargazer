#include "storage.hpp"

#define BAUD_RATE          1000000
#define WRITE_CYCLE_MAX_MS 10
#define EEPROM_SIZE        32768
#define START_ADDR         0

#include "debug.hpp"

/**
 * @brief Constructor to initialize the Storage class.
 *
 * Initializes the I2C communication with the EEPROM.
 *
 * @param i2c Pointer to the I2C instance.
 * @param sda_pin SDA pin number.
 * @param scl_pin SCL pin number.
 */
Storage::Storage(i2c_inst_t *i2c, uint sda_pin, uint scl_pin) {
    this->i2c = i2c;
    eeprom_init_i2c(i2c, sda_pin, scl_pin, BAUD_RATE, WRITE_CYCLE_MAX_MS);
}

/**
 * @brief Stores a command in EEPROM.
 *
 * Searches for an empty space in EEPROM and writes the command if space is found.
 *
 * @param command The command to store.
 *
 * @return bool Returns true if the command was successfully stored, false if EEPROM is full.
 */
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

    return write(command, page);
}

/**
 * @brief Retrieves a command from EEPROM by its ID.
 *
 * Reads EEPROM pages sequentially until the matching ID is found.
 *
 * @param command The command object to store the retrieved data.
 * @param id The unique identifier of the command.
 *
 * @return bool Returns true if the command is found and valid, false otherwise.
 */
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

/**
 * @brief Writes a command to a specific EEPROM address.
 *
 * Computes a CRC16 checksum and writes both the command and checksum to EEPROM.
 *
 * @param command The command to write.
 * @param addr The EEPROM address to write to.
 *
 * @return bool Returns true if the write operation was successful, false otherwise.
 */
bool Storage::write(Command &command, uint addr) {
    uint8_t buffer[sizeof(Command) + 3];
    buffer[sizeof(Command)] = 1;
    memcpy(buffer, &command, sizeof(Command));

    uint16_t crc = crc16(buffer, sizeof(Command));
    buffer[sizeof(Command) + 1] = crc >> 8;
    buffer[sizeof(Command) + 2] = crc & 0xFF;

    return write_page(addr, buffer, sizeof(buffer));
}

/**
 * @brief Clears the entire EEPROM memory.
 *
 * Iterates through all EEPROM pages and writes zeroes.
 */
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

/**
 * @brief Computes a CRC16 checksum.
 *
 * Uses a polynomial-based algorithm to generate a checksum for error detection.
 *
 * @param data Pointer to the data buffer.
 * @param length Size of the data buffer.
 *
 * @return uint16_t The computed CRC16 checksum.
 */
uint16_t crc16(const uint8_t *data, size_t length) {
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--) {
        x = crc >> 8 ^ *data++;
        x ^= x >> 4;
        crc = (crc << 8) ^ (static_cast<uint16_t>(x << 12)) ^ (static_cast<uint16_t>(x << 5)) ^ static_cast<uint16_t>(x);
    }
    return crc;
}
