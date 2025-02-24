#include "pico/stdlib.h" // IWYU pragma: keep

#include "PicoUart.hpp"
#include "clock.hpp"
#include "commbridge.hpp"
#include "compass.hpp"
#include "controller.hpp"
#include "debug.hpp"
#include "gps.hpp"
#include "message.hpp"

#include <memory>
#include <queue>

int main() {
    stdio_init_all();
    sleep_ms(500);
    DEBUG("Boot");

    auto uart_0 = std::make_shared<PicoUart>(0, 0, 1, 115200);
    auto uart_1 = std::make_shared<PicoUart>(1, 4, 5, 9600);
    sleep_ms(50);

    auto clock = std::make_shared<Clock>();
    auto gps = std::make_shared<GPS>(uart_1, false, true);
    auto compass = std::make_shared<Compass>(17, 16, i2c0);

    auto queue = std::make_shared<std::queue<msg::Message>>();
    auto commbridge = std::make_shared<CommBridge>(uart_0, queue);

    std::vector<uint> pins_temp{1,2,3,4};
    auto motor_horizontal = std::make_shared<StepperMotor>(pins_temp, pins_temp);
    auto motor_vertical = std::make_shared<StepperMotor>(pins_temp, pins_temp);
    Controller controller(clock, gps, compass, commbridge, motor_vertical, motor_horizontal, queue);
    for (;;) {
        controller.run();

        DEBUG("Returned from main loop");
        sleep_ms(1000);
    }
}
