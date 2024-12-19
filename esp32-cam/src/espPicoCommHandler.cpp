#include "espPicoCommHandler.hpp"

EspPicoCommHandler::EspPicoCommHandler(uart_port_t uart_num, uart_config_t uart_config) {
    this->uart_num = uart_num;
    this->uart_config = uart_config;
    this->uart_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(uart_event_t));


    uart_param_config(this->uart_num, &this->uart_config);
    uart_set_pin(this->uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(this->uart_num, UART_RING_BUFFER_SIZE, UART_RING_BUFFER_SIZE, EVENT_QUEUE_SIZE, &this->uart_event_queue, 0);
}