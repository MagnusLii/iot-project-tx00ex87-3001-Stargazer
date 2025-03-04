#include "pico/stdlib.h" // IWYU pragma: keep

#include "PicoUart.hpp"
#include "clock.hpp"
#include "commbridge.hpp"
#include "compass.hpp"
#include "controller.hpp"
#include "debug.hpp"
#include "gps.hpp"
#include "message.hpp"
#include "motor-control.hpp"
#include "stepper-motor.hpp"

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
    auto compass = std::make_shared<Compass>(i2c0, 17, 16);

    auto queue = std::make_shared<std::queue<msg::Message>>();
    auto commbridge = std::make_shared<CommBridge>(uart_0, queue);

    const std::vector<uint> pins1{2, 3, 6, 13};
    const std::vector<uint> pins2{16, 17, 18, 19};
    const int opto_horizontal = 14;
    const int opto_vertical = 28;

    auto mh = std::make_shared<StepperMotor>(pins1);
    auto mv = std::make_shared<StepperMotor>(pins2);
    auto mctrl = std::make_shared<MotorControl>(mh, mv, opto_horizontal, opto_vertical);

    Controller controller(clock, gps, compass, commbridge, mctrl, queue);
    for (;;) {
        controller.run();

        DEBUG("Returned from main loop");
        sleep_ms(1000);
    }
}
