#include "commbridge.hpp"
#include "convert.hpp"
#include "hardware/clock.hpp"
#include "message.hpp"
#include "pico/stdlib.h"
#include "uart/PicoUart.hpp"
#include <hardware/timer.h>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <vector>

#include "debug.hpp"

uint8_t checksum8(const std::string &input) {
    uint32_t sum = 0;

    for (char c : input) {
        sum += static_cast<unsigned char>(c);
    }

    return static_cast<uint8_t>(sum % 256);
}

/*
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
*/

int main() {
    stdio_init_all();
    sleep_ms(5000);
    auto queue = std::make_shared<std::queue<Message>>();
    auto uart = std::make_shared<PicoUart>(0, 0, 1, 9600);

    auto clock = std::make_shared<Clock>();

    CommBridge bridge(uart, queue);

    int count = 0;
    for (;;) {
        if (count % 2 == 0) {
            uart->send("Sleepy time\r\n");
        } else {
            uart->send("Received wakeup signal\r\n");
            bridge.read_and_parse(10000, true);
        }

        while (queue->size() > 0) {
            Message msg = queue->front();
            Message datetime_msg; 

            switch (msg.type) {
                case RESPONSE: // Received response ACK/NACK from ESP
                    break;
                case DATETIME:
                    clock->update(msg.content[0]);
                    datetime_msg = msg;
                    bridge.send(datetime_msg); // TEST
                    break;
                case ESP_INIT: // Send ACK response back to ESP
                    bridge.send(response(true));
                    break;
                case INSTRUCTIONS:
                    // TODO: Handle received instructions
                    break;
                case PICTURE:
                    // TODO: Handle taking picture
                    break;
                case DIAGNOSTICS: // Pico should not receive these
                    DEBUG("Received diagnostics for some reason");
                    break;
                default:
                    DEBUG("Unknown message type: ", msg.type);
                    break;
            }

            queue->pop();
        }

        count++;
        sleep_ms(2000);
    }

    return 0;
}
