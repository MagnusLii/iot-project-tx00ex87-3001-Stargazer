/*
 * @file crc.cpp
 * @brief Implementation of CRC-16 MODBUS algorithm.
 */

#include "crc.hpp"

/**
 * @brief Calculates the CRC-16 checksum of a string.
 * @details This function uses a CRC-16 MODBUS algorithm to calculate the checksum of a string.
 * @param input The input string.
 * @return uint16_t The CRC-16 checksum of the input string.
 */
uint16_t crc16(const std::string &input) {
    uint16_t crc = 0xFFFF;
    for (char c : input) {
        crc ^= static_cast<uint16_t>(c);
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
