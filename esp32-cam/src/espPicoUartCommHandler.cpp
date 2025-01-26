#include "espPicoUartCommHandler.hpp"
#include "debug.hpp"
#include "defines.hpp"
#include "message.hpp"
#include "driver/gpio.h"

EspPicoCommHandler::EspPicoCommHandler(uart_port_t uart_num, uart_config_t uart_config) {
    DEBUG("Disabling DEBUGS, switching UART to pico comm mode");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow for debug messages to be sent before disabling them
    esp_log_level_set("*", ESP_LOG_NONE);
    gpio_reset_pin(GPIO_NUM_0);
    gpio_reset_pin(GPIO_NUM_3);

    this->uart_num = uart_num;
    this->uart_config = uart_config;
    this->uart_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(uart_event_t));
    this->uart_received_data_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(UartReceivedData));

    ESP_ERROR_CHECK(uart_param_config(this->uart_num, &this->uart_config));
    ESP_ERROR_CHECK(uart_set_pin(this->uart_num, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(this->uart_num, UART_RING_BUFFER_SIZE, UART_RING_BUFFER_SIZE, EVENT_QUEUE_SIZE,
                                        &this->uart_event_queue, 0));
}

void EspPicoCommHandler::send_data(const char *data, size_t len) { uart_write_bytes(this->uart_num, data, len); }

// blocking
int EspPicoCommHandler::receive_data(char *buffer, size_t max_len) {
    uart_event_t event;
    if (xQueueReceive(this->uart_event_queue, (void *)&event, portMAX_DELAY)) {
        if (event.type == UART_DATA) {
            int len = uart_read_bytes(this->uart_num, (uint8_t *)buffer, max_len, portMAX_DELAY);
            return len;
        }
    }
    return 0;
}

uart_port_t EspPicoCommHandler::get_uart_num() { return this->uart_num; }

uart_config_t EspPicoCommHandler::get_uart_config() { return this->uart_config; }

QueueHandle_t EspPicoCommHandler::get_uart_event_queue_handle() { return this->uart_event_queue; }

QueueHandle_t EspPicoCommHandler::get_uart_received_data_queue_handle() { return this->uart_received_data_queue; }

TimerHandle_t EspPicoCommHandler::get_request_timer_handle() { return this->responseWaitTimer; }

bool EspPicoCommHandler::set_response_wait_timer(std::string command) {
    if (this->waitingForResponse) {
        DEBUG("Already waiting for response, ignoring new command");
        return false;
    }

    this->previousCommand = command;
    this->waitingForResponse = true;

    TimerHandle_t timer =
        xTimerCreate("responseWaitTimer", pdMS_TO_TICKS(RESPONSE_WAIT_TIME), pdFALSE, (void *)0, NULL);
    if (timer == NULL) {
        DEBUG("Failed to create response wait timer");
        return false;
    }

    this->responseWaitTimer = timer;
    xTimerStart(this->responseWaitTimer, 0);

    return true;
}

void EspPicoCommHandler::disable_response_wait_timer() {
    this->waitingForResponse = false;
    previousCommand.clear();

    xTimerDelete(this->responseWaitTimer, 0);
}

void EspPicoCommHandler::set_request_handler(std::shared_ptr<RequestHandler> requestHandler) {
    this->requestHandler = requestHandler;
}