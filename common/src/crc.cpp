#include "crc.hpp"

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
