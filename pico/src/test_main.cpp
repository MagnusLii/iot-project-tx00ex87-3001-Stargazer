#include "commbridge.hpp"
#include "convert.hpp"
#include "devices/gps.hpp"
#include "hardware/clock.hpp"
#include "devices/compass.hpp"
#include "message.hpp"
#include "pico/stdlib.h" // IWYU pragma: keep
#include "uart/PicoUart.hpp"
#include <hardware/timer.h>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <vector>
#include "sleep_functions.hpp"

#include "debug.hpp"

static bool awake;

static void alarm_sleep_callback(uint alarm_id) {
    printf("alarm woke us up\n");
    uart_default_tx_wait_blocking();
    awake = true;
    hardware_alarm_set_callback(alarm_id, NULL);
    hardware_alarm_unclaim(alarm_id);
}

int main() {
    stdio_init_all();
    sleep_ms(5000);
    DEBUG("Start\r\n");

    awake = false;
    if (sleep_goto_sleep_for(10000, &alarm_sleep_callback)) {
        while(!awake)
            DEBUG("bruh");
    }

    sleep_power_up();
    DEBUG("Hello");

    while(true) {

    };
    /*
    auto queue = std::make_shared<std::queue<msg::Message>>();
    auto uart_0 = std::make_shared<PicoUart>(0, 0, 1, 115200);
    auto uart_1 = std::make_shared<PicoUart>(1, 4, 5, 9600);

    auto clock = std::make_shared<Clock>();
    auto gps = std::make_unique<GPS>(uart_1, false, true);
    //Compass compass(17, 16, i2c0);
    //compass.init();

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

        bridge.send(msg::datetime_request());
        sleep_ms(1000);

        //heading = compass.getHeading();
        //DEBUG("Heading: ", heading);
        bridge.send(msg::esp_init(true));
        sleep_ms(1000);
        bridge.send(msg::instructions(3, 15, 2));
        sleep_ms(1000);
        bridge.send(msg::picture(3, 15));
        sleep_ms(1000);
        const std::string ssid = "Stargazer";
        const std::string password = "stargazer";
        bridge.send(msg::wifi(ssid, password));
        sleep_ms(1000);
        const std::string server_addr = "237.84.2.178";
        bridge.send(msg::server(server_addr));
        sleep_ms(1000);
        const std::string api_token = "stargazer123123231";
        bridge.send(msg::api(api_token));
        sleep_ms(1000);

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
                    bridge.send(msg::response(true));
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
    */
    return 0;
}
