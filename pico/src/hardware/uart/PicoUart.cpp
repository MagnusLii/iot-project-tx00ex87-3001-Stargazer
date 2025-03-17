/**
 * @file PicoUart.cpp
 * @brief Implementation of the PicoUart class for UART communication on Raspberry Pi Pico.
 */

#include "PicoUart.hpp"
#include <cstring>
#include <hardware/gpio.h>

/** Static pointers to PicoUart instances for UART0 and UART1 */
static PicoUart *pu0;
static PicoUart *pu1;

/**
 * @brief UART0 interrupt handler.
 * Calls the receive and transmit interrupt handlers for the PicoUart instance.
 */
void pico_uart0_handler(void) {
    if (pu0) {
        pu0->uart_irq_rx();
        pu0->uart_irq_tx();
    } else
        irq_set_enabled(UART0_IRQ, false);
}

/**
 * @brief UART1 interrupt handler.
 * Calls the receive and transmit interrupt handlers for the PicoUart instance.
 */
void pico_uart1_handler(void) {
    if (pu1) {
        pu1->uart_irq_rx();
        pu1->uart_irq_tx();
    } else
        irq_set_enabled(UART1_IRQ, false);
}

/**
 * @brief Constructs a PicoUart object and initializes UART communication.
 *
 * @param uart_nr UART number (0 or 1).
 * @param tx_pin GPIO pin for TX.
 * @param rx_pin GPIO pin for RX.
 * @param speed Baud rate.
 * @param stop Number of stop bits.
 * @param tx_size Size of the transmit buffer.
 * @param rx_size Size of the receive buffer.
 */
PicoUart::PicoUart(int uart_nr, int tx_pin, int rx_pin, int speed, int stop, int tx_size, int rx_size)
    : tx(tx_size), rx(rx_size), speed{speed} {
    irqn = uart_nr == 0 ? UART0_IRQ : UART1_IRQ;
    uart = uart_nr == 0 ? uart0 : uart1;
    if (uart_nr == 0) {
        pu0 = this;
    } else {
        pu1 = this;
    }

    // ensure that we don't get any interrupts from the uart during configuration
    irq_set_enabled(irqn, false);

    // Set up our UART with the required speed.
    uart_init(uart, speed);
    uart_set_format(uart, 8, stop, UART_PARITY_NONE);

    // Set the TX and RX pins by using the function select on the GPIO
    // See datasheet for more information on function select
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);

    irq_set_exclusive_handler(irqn, uart_nr == 0 ? pico_uart0_handler : pico_uart1_handler);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(uart, true, false);
    // enable UART0 interrupts on NVIC
    irq_set_enabled(irqn, true);
}

/**
 * @brief Reads data from the receive buffer.
 *
 * @param buffer Pointer to the buffer where received data will be stored.
 * @param size Maximum number of bytes to read.
 * @return Number of bytes actually read.
 */
int PicoUart::read(uint8_t *buffer, int size) {
    int count = 0;
    while (count < size && !rx.empty()) {
        *buffer++ = rx.get();
        ++count;
    }
    return count;
}

/**
 * @brief Writes data to the transmit buffer and enables the transmit interrupt.
 *
 * @param buffer Pointer to the data to be written.
 * @param size Number of bytes to write.
 * @return Number of bytes actually written.
 */
int PicoUart::write(const uint8_t *buffer, int size) {
    int count = 0;
    // write data to ring buffer
    while (count < size && !tx.full()) {
        tx.put(*buffer++);
        ++count;
    }
    // disable interrupts on NVIC while managing transmit interrupts
    irq_set_enabled(irqn, false);
    // if transmit interrupt is not enabled we need to enable it and give fifo an initial filling
    if (!(uart_get_hw(uart)->imsc & (1 << UART_UARTIMSC_TXIM_LSB))) {
        // enable transmit interrupt
        uart_set_irq_enables(uart, true, true);
        // fifo requires initial filling
        uart_irq_tx();
    }
    // enable interrupts on NVIC
    irq_set_enabled(irqn, true);

    return count;
}

/**
 * @brief Sends a C string to the UART.
 *
 * @param str Pointer to the string to be sent.
 * @return Always returns 0.
 */
int PicoUart::send(const char *str) {
    write(reinterpret_cast<const uint8_t *>(str), strlen(str));
    return 0;
}

/**
 * @brief Sends a std::string to the UART.
 *
 * @param str The string to be sent.
 * @return Always returns 0.
 */
int PicoUart::send(const std::string &str) {
    write(reinterpret_cast<const uint8_t *>(str.c_str()), str.length());
    return 0;
}

/**
 * @brief Flushes the receive buffer.
 *
 * @return Number of bytes removed from the buffer.
 */
int PicoUart::flush() {
    int count = 0;
    while (!rx.empty()) {
        (void)rx.get();
        ++count;
    }
    return count;
}

/**
 * @brief UART receive interrupt handler.
 * Reads available data from the UART hardware and stores it in the receive buffer.
 */
void PicoUart::uart_irq_rx() {
    while (uart_is_readable(uart)) {
        uint8_t c = uart_getc(uart);
        // ignoring return value for now
        rx.put(c);
    }
}

/**
 * @brief UART transmit interrupt handler.
 * Sends available data from the transmit buffer to the UART hardware.
 * Disables the transmit interrupt if the buffer is empty.
 */
void PicoUart::uart_irq_tx() {
    while (!tx.empty() && uart_is_writable(uart)) {
        uart_get_hw(uart)->dr = tx.get();
    }

    if (tx.empty()) {
        // disable tx interrupt if transmit buffer is empty
        uart_set_irq_enables(uart, true, false);
    }
}
