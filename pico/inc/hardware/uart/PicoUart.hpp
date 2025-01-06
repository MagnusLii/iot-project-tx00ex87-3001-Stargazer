#pragma once

#include <hardware/uart.h>
#include <hardware/irq.h>
#include <string>
#include "RingBuffer.hpp"

class PicoUart {
    friend void pico_uart0_handler(void);
    friend void pico_uart1_handler(void);
public:
    PicoUart(int uart_nr, int tx_pin, int rx_pin, int speed, int stop = 1, int tx_size = 256, int rx_size = 256);
    PicoUart(const PicoUart &) = delete; // prevent copying because each instance is associated with a HW peripheral
    int read(uint8_t *buffer, int size);
    int write(const uint8_t *buffer, int size);
    int send(const char *str);
    int send(const std::string &str);
    int flush();
private:
    void uart_irq_rx();
    void uart_irq_tx();
    RingBuffer tx;
    RingBuffer rx;
    uart_inst_t *uart;
    int irqn;
    int speed;
};

