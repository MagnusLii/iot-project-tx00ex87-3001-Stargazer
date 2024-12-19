#include "espPicoUartCommHandler.hpp"

EspPicoCommHandler::EspPicoCommHandler(uart_port_t uart_num, uart_config_t uart_config) {
    this->uart_num = uart_num;
    this->uart_config = uart_config;
    this->uart_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(uart_event_t));

    uart_param_config(this->uart_num, &this->uart_config);
    uart_set_pin(this->uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(this->uart_num, UART_RING_BUFFER_SIZE, UART_RING_BUFFER_SIZE, EVENT_QUEUE_SIZE,
                        &this->uart_event_queue, 0);
}

void EspPicoCommHandler::send_data(const char *data, size_t len) { uart_write_bytes(this->uart_num, data, len); }

int EspPicoCommHandler::receive_data(char *buffer, size_t max_len) {
    uart_event_t event;
    if (xQueueReceive(this->uart_event_queue, (void *)&event, portMAX_DELAY)) {
        if (event.type == UART_DATA) {
            int len = uart_read_bytes(this->uart_num, (uint8_t *)buffer, max_len, portMAX_DELAY);
            return len; // Return the number of bytes read
        }
    }
    return 0; // Return 0 if no data was received
}

uart_port_t EspPicoCommHandler::get_uart_num() { return this->uart_num; }

uart_config_t EspPicoCommHandler::get_uart_config() { return this->uart_config; }

QueueHandle_t EspPicoCommHandler::get_uart_event_queue() { return this->uart_event_queue; }