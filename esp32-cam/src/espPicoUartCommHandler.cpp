#include "espPicoUartCommHandler.hpp"
#include "debug.hpp"
#include "defines.hpp"
#include "driver/gpio.h"
#include "message.hpp"

/**
 * @brief Constructs an EspPicoCommHandler object for UART communication.
 *
 * This constructor initializes the UART interface with the provided configuration,
 * sets up event queues for UART communication, and installs the UART driver.
 *
 * @param uart_num The UART port to be used for communication.
 * @param uart_config The UART configuration settings.
 *
 * @note Initializes two queues for UART events and received data. Configures UART pins 
 *       for the specified UART port and installs the UART driver for communication.
 *       Uses `ESP_ERROR_CHECK` to ensure that the UART setup is successful.
 */
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

/**
 * @brief Destructor for the EspPicoCommHandler class.
 *
 * Cleans up the resources allocated by the EspPicoCommHandler. This includes deleting the UART event queue 
 * and received data queue, and performing any necessary cleanup for the UART driver.
 *
 * @return void
 *
 * @note This destructor ensures that any dynamically allocated resources, such as the event queues, 
 *       are properly freed and that the UART driver is uninstalled.
 */
EspPicoCommHandler::~EspPicoCommHandler() {
    if (this->uart_event_queue) {
        vQueueDelete(this->uart_event_queue);  // Delete UART event queue
    }
    if (this->uart_received_data_queue) {
        vQueueDelete(this->uart_received_data_queue);  // Delete UART received data queue
    }
    // Uninstall the UART driver to release resources
    ESP_ERROR_CHECK(uart_driver_delete(this->uart_num));
}

/**
 * @brief Sends data over UART.
 *
 * This function transmits the specified data over the configured UART interface.
 *
 * @param data A pointer to the data to be sent.
 * @param len The length of the data to be sent.
 *
 * @note This function uses `uart_write_bytes` to send the data to the UART port.
 */
void EspPicoCommHandler::send_data(const char *data, const size_t len) { uart_write_bytes(this->uart_num, data, len); }

/**
 * @brief Receives data from the UART interface.
 *
 * This function waits for a UART event and, upon receiving data, reads it into 
 * the provided buffer.
 *
 * @param buffer A pointer to the buffer where the received data will be stored.
 * @param max_len The maximum number of bytes to read into the buffer.
 *
 * @return int Returns:
 * @return        - The number of bytes received.
 * @return        - `0` if no data is received or an error occurs.
 *
 * @note The function blocks indefinitely until a UART data event is received.
 *       The `uart_event_queue` must be set up correctly for this function to work.
 */
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

/**
 * @brief Retrieves the UART port number.
 *
 * This function returns the UART port number used by the `EspPicoCommHandler` object.
 *
 * @return uart_port_t The UART port number.
 */
uart_port_t EspPicoCommHandler::get_uart_num() { return this->uart_num; }

/**
 * @brief Retrieves the UART configuration.
 *
 * This function returns the UART configuration settings used by the 
 * `EspPicoCommHandler` object.
 *
 * @return uart_config_t The UART configuration structure.
 */
uart_config_t EspPicoCommHandler::get_uart_config() { return this->uart_config; }

/**
 * @brief Retrieves the UART event queue handle.
 *
 * This function returns the handle to the queue used for UART events.
 *
 * @return QueueHandle_t The handle to the UART event queue.
 */
QueueHandle_t EspPicoCommHandler::get_uart_event_queue_handle() { return this->uart_event_queue; }

/**
 * @brief Retrieves the UART received data queue handle.
 *
 * This function returns the handle to the queue used for storing received UART data.
 *
 * @return QueueHandle_t The handle to the UART received data queue.
 */
QueueHandle_t EspPicoCommHandler::get_uart_received_data_queue_handle() { return this->uart_received_data_queue; }

/**
 * @brief Sets the status for waiting for a response.
 *
 * This function updates the flag indicating whether the handler is currently 
 * waiting for a response.
 *
 * @param status A boolean value:
 *               - `true` to set the status to waiting for a response.
 *               - `false` to set the status to not waiting for a response.
 */
void EspPicoCommHandler::set_waiting_for_response(bool status) { this->waitingForResponse = status; }

/**
 * @brief Retrieves the current status of waiting for a response.
 *
 * This function returns the flag indicating whether the handler is currently 
 * waiting for a response.
 *
 * @return bool Returns:
 * @return        - `true` if the handler is waiting for a response.
 * @return        - `false` if the handler is not waiting for a response.
 */
bool EspPicoCommHandler::get_waiting_for_response() { return this->waitingForResponse; }

/**
 * @brief Sends a message and waits for a response with retries.
 *
 * This function sends a message over UART and waits for a response. If no response
 * is received, it retries up to a specified number of times. It updates the waiting 
 * status and returns the result.
 *
 * @param data A pointer to the data to be sent.
 * @param len The length of the data to be sent.
 *
 * @return int Returns:
 * @return        - `0` if a response is received within the retry limit.
 * @return        - `1` if no response is received after the maximum retries.
 *
 * @note The function will wait for a response for a predefined amount of time
 *       between retries (`PICO_RESPONSE_WAIT_TIME`). The retry limit is defined by `RETRIES`.
 */
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

/**
 * @brief Checks if the received message is a confirmation response.
 *
 * This function processes the received UART data to determine if it is a valid 
 * confirmation message. If the message type is a response and indicates success 
 * (i.e., the content is "1"), it updates the waiting status. 
 *
 * @param receivedData The received UART data to be checked.
 *
 * @note The function assumes that the received data can be successfully 
 *       converted into a `msg::Message`. If the message is of type `RESPONSE` 
 *       and contains a content of "1", it indicates a positive confirmation.
 */
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

/**
 * @brief Sends an acknowledgment message.
 *
 * This function creates a response message based on the provided acknowledgment 
 * status (`true` or `false`), converts it to a string, and sends it over UART.
 *
 * @param ack A boolean indicating the acknowledgment status:
 *            - `true` for a positive acknowledgment.
 *            - `false` for a negative acknowledgment.
 *
 * @note The message is generated using the `msg::response` function, 
 *       converted to a string, and then sent using the `send_data` method.
 */
void EspPicoCommHandler::send_ACK_msg(const bool ack) {
    msg::Message msg = msg::response(ack);
    std::string string;
    convert_to_string(msg, string);
    this->send_data(string.c_str(), string.length());
}

/**
 * @brief Finds the position of the first occurrence of a target character in a buffer.
 *
 * This function iterates through the provided buffer to locate the first occurrence 
 * of the specified target character. It returns the index of the first match or 
 * a special error code if the character is not found or if the buffer is invalid.
 *
 * @param data_buffer A pointer to the data buffer to search.
 * @param data_buffer_len The length of the data buffer.
 * @param target The target character to search for in the buffer.
 *
 * @return int Returns:
 * @return        - The index of the first occurrence of the target character, if found.
 * @return        - `-1` if the data buffer is invalid (nullptr or empty).
 * @return        - `-2` if the target character is not found in the buffer.
 *
 * @note This function performs a linear search through the buffer, and the search 
 *       will stop as soon as the first occurrence of the target is found.
 */
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

/**
 * @brief Extracts a message from a UART buffer.
 *
 * This function searches for a message enclosed between the '$' and ';' characters 
 * in the given buffer. If a valid message is found, it is extracted into the 
 * `extracted_msg` structure, and the original buffer is updated by removing the 
 * extracted message. The function returns an error code in case of any issues, 
 * such as an invalid start or end position, or if the message is too long.
 *
 * @param data_buffer A pointer to the UART data buffer containing the message.
 * @param data_buffer_len A pointer to the length of the data buffer.
 * @param extracted_msg A pointer to a structure where the extracted message will be stored.
 *
 * @return int Returns:
 * @return        - `0` on successful extraction.
 * @return        - `-1` if no start of message (`$`) is found.
 * @return        - `-2` if no end of message (`;`) is found.
 * @return        - `-3` if the start position is after the end position.
 * @return        - `-4` if the message is too long.
 *
 * @note This function modifies the original `data_buffer` by removing the extracted 
 *       message, and updates the `data_buffer_len` accordingly.
 */
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
