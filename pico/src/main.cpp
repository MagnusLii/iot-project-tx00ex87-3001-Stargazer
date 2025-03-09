#include "pico/stdlib.h" // IWYU pragma: keep

#include "PicoUart.hpp"
#include "clock.hpp"
#include "commbridge.hpp"
#include "compass.hpp"
#include "controller.hpp"
#include "debug.hpp"
#include "gps.hpp"
#include "storage.hpp"
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
    DEBUG("UART0 initialized");
    auto uart_1 = std::make_shared<PicoUart>(1, 4, 5, 9600);
    DEBUG("UART1 initialized");
    sleep_ms(50);

    auto clock = std::make_shared<Clock>();
    DEBUG("Clock initialized");
    auto gps = std::make_shared<GPS>(uart_1, false, true);
    DEBUG("GPS initialized");
    auto compass = std::make_shared<Compass>(i2c0, 17, 16);
    DEBUG("Compass initialized");
    auto storage = std::make_shared<Storage>(i2c1, 26, 27);

    auto queue = std::make_shared<std::queue<msg::Message>>();
    DEBUG("Queue initialized");
    auto commbridge = std::make_shared<CommBridge>(uart_0, queue);
    DEBUG("CommBridge initialized");

    const std::vector<uint> pins1{6, 7, 8, 9};
    DEBUG("Stepper motor pins initialized");
    const std::vector<uint> pins2{18, 19, 20, 21};
    DEBUG("Stepper motor pins initialized");
    const int opto_horizontal = 10;
    const int opto_vertical = 15;

    auto mh = std::make_shared<StepperMotor>(pins1);
    DEBUG("Stepper motor initialized");
    auto mv = std::make_shared<StepperMotor>(pins2);
    DEBUG("Stepper motor initialized");
    auto mctrl = std::make_shared<MotorControl>(mh, mv, opto_horizontal, opto_vertical);
    DEBUG("MotorControl initialized");

    Controller controller(clock, gps, compass, commbridge, mctrl, storage, queue);
    DEBUG("Controller initialized");
    for (;;) {
        controller.run();

        DEBUG("Returned from main loop");
        sleep_ms(1000);
    }
}
