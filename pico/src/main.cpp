#include "PicoUart.hpp"
#include "gps.hpp"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include <iostream>
#include <memory>

#include "debug.hpp"

int main() {
    stdio_init_all();
    sleep_ms(5000);
    DEBUG("Booted");

    auto uart = std::make_shared<PicoUart>(0, 0, 1, 9600);

    uint8_t buffer[256] = {0};

    DEBUG("Sleepy time set to", 50, "ms");
    for (;;) {
        
        uart->read(buffer, sizeof(buffer));
        if (buffer[0] != 0) {
            std::cout << buffer;
            std::fill_n(buffer, sizeof(buffer), 0);
        }
        
        sleep_ms(50);
    }

    return 0;
}
