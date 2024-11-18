#include "PicoUart.hpp"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include <iostream>
#include <memory>

int main() {
    stdio_init_all();
    std::cout << "Boot" << std::endl;

    auto uart = std::make_shared<PicoUart>(0, 0, 1, 9600);

    uint8_t buffer[256] = {0};

    for (;;) {
        
        uart->read(buffer, sizeof(buffer));
        if (buffer[0] != 0) {
            std::cout << buffer;
            std::fill_n(buffer, sizeof(buffer), 0);
        }
        
        //std::cout << "Hello" << std::endl;
        sleep_ms(50);
    }

    return 0;
}
