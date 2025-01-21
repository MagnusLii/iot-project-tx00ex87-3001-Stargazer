#include "commbridge.hpp"
#include "convert.hpp"
#include "devices/gps.hpp"
#include "hardware/clock.hpp"
#include "devices/compass.hpp"
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

int main() {
    stdio_init_all();
    sleep_ms(5000);
    DEBUG("Start\r\n");
    auto queue = std::make_shared<std::queue<msg::Message>>();
    auto uart_0 = std::make_shared<PicoUart>(0, 0, 1, 9600);
    auto uart_1 = std::make_shared<PicoUart>(1, 4, 5, 9600);

    auto clock = std::make_shared<Clock>();
    auto gps = std::make_unique<GPS>(uart_1, false, true);
    Compass compass(17, 16, i2c0);
    compass.init();

    CommBridge bridge(uart_0, queue);

    sleep_ms(50);
    gps->set_mode(GPS::Mode::FULL_ON);

    bool fix = false;
    float heading = 0;
    for (;;) {
        Coordinates coords = gps->get_coordinates();
        if (coords.status) {
            DEBUG("Latitude: ", coords.latitude);
            DEBUG("Longitude: ", coords.longitude);
            if (!fix) {
                fix = true;
                gps->set_mode(GPS::Mode::STANDBY);
            }
        } else if (!fix) {
            gps->locate_position(5);
        }

        heading = compass.getHeading();
        DEBUG("Heading: ", heading);

        DEBUG("Received wakeup signal\r\n");
        bridge.read_and_parse(10000, true);

        while (queue->size() > 0) {
            msg::Message msg = queue->front();

            switch (msg.type) {
                case msg::RESPONSE: // Received response ACK/NACK from ESP
                    DEBUG("Received response");
                    break;
                case msg::DATETIME:
                    DEBUG("Received datetime");
                    clock->update(msg.content[0]);
                    break;
                case msg::ESP_INIT: // Send ACK response back to ESP
                    DEBUG("Received ESP init");
                    bridge.send(msg::response(true));
                    break;
                case msg::INSTRUCTIONS:
                    DEBUG("Received instructions");
                    for (auto it = msg.content.begin(); it != msg.content.end(); ++it) {
                        DEBUG("Instruction: ", *it);
                    }
                    bridge.send(msg::response(true));
                    break;
                case msg::PICTURE: // Pico should not receive these
                    DEBUG("Received picture msg for some reason");
                    break;
                case msg::DIAGNOSTICS: // Pico should not receive these
                    DEBUG("Received diagnostics msg for some reason");
                    break;
                default:
                    DEBUG("Unknown message type: ", msg.type);
                    break;
            }

            queue->pop();
        }

        sleep_ms(5000);

        if (clock->is_synced()) {
            datetime_t now = clock->get_datetime();
            DEBUG("Current time: ", now.year, "-", unsigned(now.month), "-", unsigned(now.day), " ", unsigned(now.hour),
                  ":", unsigned(now.min), ":", unsigned(now.sec));
        }
    }

    return 0;
}
