#include "espPicoUartCommHandler.hpp"
#include "debug.hpp"
#include "defines.hpp"
#include "driver/gpio.h"
#include "message.hpp"

EspPicoCommHandler::EspPicoCommHandler(uart_port_t uart_num, uart_config_t uart_config) {
    this->uart_num = uart_num;
    this->uart_config = uart_config;
    this->uart_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(uart_event_t));
    this->uart_received_data_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(UartReceivedData));

    ESP_ERROR_CHECK(uart_param_config(this->uart_num, &this->uart_config));
    ESP_ERROR_CHECK(uart_set_pin(this->uart_num, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(this->uart_num, UART_RING_BUFFER_SIZE, UART_RING_BUFFER_SIZE, EVENT_QUEUE_SIZE,
                                        &this->uart_event_queue, 0));
}

void EspPicoCommHandler::send_data(const char *data, const size_t len) { uart_write_bytes(this->uart_num, data, len); }

// blocking
int EspPicoCommHandler::receive_data(char *buffer, const size_t max_len) {
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

void EspPicoCommHandler::set_waiting_for_response(bool status) { this->waitingForResponse = status; }

bool EspPicoCommHandler::get_waiting_for_response() { return this->waitingForResponse; }

int EspPicoCommHandler::send_msg_and_wait_for_response(const char *data, const size_t len) {
    DEBUG("Sending message and waiting for response");
    int retries = 0;
    this->set_waiting_for_response(true);
    while (this->get_waiting_for_response() && retries < RETRIES) {
        DEBUG("Sending message and waiting for response");
        this->send_data(data, len);
        retries++;
        vTaskDelay(pdMS_TO_TICKS(PICO_RESPONSE_WAIT_TIME));
    }

    // Didn't receive confirmation response
    if (retries >= RETRIES) {
        DEBUG("Failed to receive confirmation response after ", retries, " attempts");
        this->set_waiting_for_response(false);
        return 1;
    }
    return 0;
}

void EspPicoCommHandler::check_if_confirmation_msg(const UartReceivedData &receivedData) {
    DEBUG("Checking if confirmation message");
    std::string string(receivedData.buffer, receivedData.len);
    msg::Message msg;

    if (convert_to_message(string, msg) == 0) {
        if (msg.type == msg::MessageType::RESPONSE) {
            if (msg.content[0] == "1") {
                DEBUG("Pico Confirmation response returned true");
                this->set_waiting_for_response(false);
            } else {
                DEBUG("Pico Confirmation response returned false");
            }
        }
    }
}

void EspPicoCommHandler::send_ACK_msg(const bool ack) {
    msg::Message msg = msg::response(ack);
    std::string string;
    convert_to_string(msg, string);
    this->send_data(string.c_str(), string.length());
}

int find_first_char_position(const char *data_buffer, const size_t data_buffer_len, const char target) {
    DEBUG("buffer length: ", data_buffer_len);
    if (data_buffer == nullptr || data_buffer_len == 0) {
        DEBUG("Invalid data buffer");
        return -1; // Invalid array
    }
    DEBUG("Finding char: ", target);

    for (size_t i = 0; i < data_buffer_len; ++i) {
        if (data_buffer[i] == target) {
            return static_cast<int>(i); // Return position if found
        }
    }

    DEBUG("Char not found");
    return -2; // char not found
}

int extract_msg_from_uart_buffer(char *data_buffer, size_t *data_buffer_len, UartReceivedData *extracted_msg) {
    DEBUG("Extracting message from buffer: ", data_buffer);
    DEBUG("Buffer length: ", *data_buffer_len);
    int start_pos = find_first_char_position(data_buffer, *data_buffer_len, '$');
    if (start_pos < 0) {
        DEBUG("No start of message found");
        DEBUG("Start position: ", start_pos);
        return -1; // No message found
    }

    int end_pos = find_first_char_position(data_buffer, *data_buffer_len, ';');
    if (end_pos < 0) {
        DEBUG("No end of message found");
        DEBUG("End position: ", end_pos);
        return -2; // No end of message found
    }

    if (start_pos >= end_pos) {
        DEBUG("Start position is after end position");
        DEBUG("Start position: ", start_pos, " End position: ", end_pos);
        return -3; // Start position is after end position
    }

    // Extract the message from the buffer
    int msg_length = end_pos - start_pos + 1;
    if (msg_length > LONGEST_COMMAND_LENGTH) {
        DEBUG("Message too long");
        return -4; // Message too long
    }

    // Copy the message to extracted_msg_buffer
    strncpy(extracted_msg->buffer, data_buffer + start_pos, msg_length);
    extracted_msg->buffer[msg_length] = '\0'; // Null terminate the string
    extracted_msg->len = msg_length;

    // remove the message from the buffer
    int chars_to_remove = end_pos - start_pos + 1;
    int index = start_pos;
    while (index < *data_buffer_len - chars_to_remove) {
        data_buffer[index] = data_buffer[index + chars_to_remove];
        index++;
    }
    data_buffer[index] = '\0';

    // Update the data buffer length
    *data_buffer_len -= chars_to_remove;
    DEBUG("Buffer after extraction: ", data_buffer);
    DEBUG("Buffer length after extraction: ", *data_buffer_len);
    return 0; // Success
}
