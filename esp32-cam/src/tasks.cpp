#include "tasks.hpp"
#include "TaskPriorities.hpp"
#include "camera.hpp"
#include "debug.hpp"
#include "espPicoUartCommHandler.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "jsonParser.hpp"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "message.hpp"
#include "nvs_flash.h"
#include "requestHandler.hpp"
#include "sd-card.hpp"
#include "sdkconfig.h"
#include "socket.h"
#include "timesync-lib.hpp"
#include "wireless.hpp"
#include <memory>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "testMacros.hpp"

struct Handlers {
    std::shared_ptr<WirelessHandler> wirelessHandler;
    std::shared_ptr<SDcard> sdcard;
    std::shared_ptr<RequestHandler> requestHandler;
    std::shared_ptr<EspPicoCommHandler> uartCommHandler;
    std::shared_ptr<Camera> cameraPtr;
};

// ----------------------------------------------------------
// ----------------SUPPORTING-FUNCTIONS----------------------
// ----------------------------------------------------------

/**
 * Callback function triggered by the timer to send a GET request for user instructions.
 *
 * @param timer - The timer handle that triggered this callback. It is used to retrieve the associated
 *                `RequestHandler` instance to interact with the web server request queue.
 *                The `RequestHandler` instance is retrieved from the timer's user data.
 *
 * This function retrieves the `RequestHandler` object using the timer's ID, sends the user instructions
 * GET request to the web server request queue, and logs that the request has been sent.
 */
void get_request_timer_task(void *pvParameters) {
    RequestHandler *requestHandler = (RequestHandler *)pvParameters;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        xQueueSend(requestHandler->getWebSrvRequestQueue(), requestHandler->getUserInstructionsGETRequestptr(), 0);
        DEBUG("GET request sent");
    }
}

// ----------------------------------------------------------
// ------------------------TASKS-----------------------------
// ----------------------------------------------------------

void init_task(void *pvParameters) {
    Handlers handlers;

    EspPicoCommHandler uartCommHandler(UART_NUM_0);
    WirelessHandler wifi;
    handlers.wirelessHandler = std::make_shared<WirelessHandler>(wifi);
    handlers.uartCommHandler = std::make_shared<EspPicoCommHandler>(uartCommHandler);

    SDcard sdcard("/sdcard");
    while (sdcard.get_sd_card_status() != ESP_OK) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        sdcard.mount_sd_card("/sdcard");
    }

    handlers.sdcard = std::make_shared<SDcard>(sdcard);

    wifi.connect(WIFI_SSID, WIFI_PASSWORD);
    do {
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Give the wifi some time to connect
        if (!wifi.isConnected()) {
            wifi.disconnect();
            wifi.connect(WIFI_SSID, WIFI_PASSWORD);
        }
    } while (!wifi.isConnected());
    DEBUG("Connected to wifi");

    RequestHandler requestHandler(WEB_SERVER, WEB_PORT, WEB_TOKEN, std::make_shared<WirelessHandler>(wifi),
                                  std::make_shared<SDcard>(sdcard));
    DEBUG("RequestHandler created");

    handlers.requestHandler = std::make_shared<RequestHandler>(requestHandler);

    Camera cameraPtr(std::make_shared<SDcard>(sdcard), requestHandler.getWebSrvRequestQueue());

    handlers.cameraPtr = std::make_shared<Camera>(cameraPtr);

    xTaskCreate(send_request_to_websrv_task, "send_request_to_websrv_task", 8192, &handlers, TaskPriorities::HIGH,
                nullptr);

    xTaskCreate(get_request_timer_task, "get_request_timer_task", 4096, &requestHandler, TaskPriorities::LOW, nullptr);

    xTaskCreate(uart_read_task, "uart_read_task", 4096, &uartCommHandler, TaskPriorities::ABSOLUTE, nullptr);
    xTaskCreate(handle_uart_data_task, "handle_uart_data_task", 4096, &uartCommHandler, TaskPriorities::HIGH, nullptr);

    // xTaskCreate(take_picture_and_save_to_sdcard_in_loop_task, "take_picture_and_save_to_sdcard_in_loop_task", 4096,
    //             (void *)&cameraPtr, TaskPriorities::HIGH, NULL);

    int counter = 0;
    while (true) {
        counter++;
        DEBUG("Init task running, counter: ", counter);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Sends requests to the web server
void send_request_to_websrv_task(void *pvParameters) {
    DEBUG("send_request_to_websrv_task started");
    Handlers *handlers = (Handlers *)pvParameters;

    RequestHandler *requestHandler = handlers->requestHandler.get();
    EspPicoCommHandler *uartCommHandler = handlers->uartCommHandler.get();

    // Used for server communication
    QueueMessage request;
    QueueMessage response;
    std::vector<std::string> parsed_results;
    std::vector<std::string> key_names;

    // Used for sending instructions to the Pico
    Message uart_msg;
    std::string uart_msg_str;
    int returnval;

    while (true) {
        // Wait for a request to be available
        // TODO: add mutex
        if (xQueueReceive(requestHandler->getWebSrvRequestQueue(), &request, portMAX_DELAY) == pdTRUE) {
            switch (request.requestType) {
                case RequestType::GET_COMMANDS:
                    DEBUG("GET_COMMANDS request received");
                    requestHandler->sendRequest(request, &response);

                    // Parse the response
                    DEBUG("Response: ", response.str_buffer);
                    returnval = parse_json(response.str_buffer, response.buffer_length, parsed_results);
                    DEBUG("Return value from parse_json: ", returnval);
                    // DEBUG("Parsed results: ", parsed_results[0].c_str(), " ", parsed_results[1].c_str(), " ", parsed_results[2].c_str());

                    // create instructions uart message
                    uart_msg = instructions(parsed_results[0], parsed_results[1], parsed_results[2]);
                    convert_to_string(uart_msg, uart_msg_str);

                    // Send the message to the Pico
                    uartCommHandler->send_data(uart_msg_str.c_str(), uart_msg_str.length());

                    // TODO: Add timer to wait for response

                    // Clear variables
                    parsed_results.clear();
                    response.buffer_length = 0;
                    response.str_buffer[0] = '\0';
                    uart_msg_str.clear();

                    break;
                case RequestType::POST_IMAGE:
                    DEBUG("POST_IMAGE request received");
                    // Create a POST request
                    // Add image link to the request
                    // Send the request and enqueue the response
                    break;
                case RequestType::POST_DIAGNOSTICS:
                    DEBUG("POST_DIAGNOSTICS request received");
                    // Create a POST request
                    // Add diagnostics data to the request
                    // Send the request and enqueue the response
                    break;
                default:
                    DEBUG("Unknown request type received");
                    break;
            }
        }
    }
}

// Run on highest prio.
// Reads uart and enqueues received buffer to queue.
void uart_read_task(void *pvParameters) {
    DEBUG("uart_read_task started");
    EspPicoCommHandler *uartCommHandler = (EspPicoCommHandler *)pvParameters;
    uart_event_t event;

    UartReceivedData receivedData;
    int retries = 0;

    while (true) {
        if (xQueueReceive(uartCommHandler->get_uart_event_queue_handle(), (void *)&event, portMAX_DELAY)) {

            switch (event.type) {
                case UART_DATA:
                    receivedData.len = uart_read_bytes(uartCommHandler->get_uart_num(), (uint8_t *)receivedData.buffer,
                                                       sizeof(receivedData.buffer), pdMS_TO_TICKS(10));
                    receivedData.buffer[receivedData.len] = '\0'; // Null-terminate the received string
                                                                  // TODO: Process the received data

                    while (xQueueSend(uartCommHandler->get_uart_received_data_queue_handle(), &receivedData, 0) !=
                               pdTRUE &&
                           retries < RETRIES) {
                        DEBUG("Failed to enqueue received data, retrying...");
                        retries++;
                    }
                    if (retries >= RETRIES) { DEBUG("Failed to enqueue received data after ", RETRIES, " attempts"); }
                    retries = 0;
                    break;

                default:
                    DEBUG("Unknown event type received");
                    uart_flush_input(uartCommHandler->get_uart_num());
                    xQueueReset(uartCommHandler->get_uart_event_queue_handle());
                    break;
            }
        }
    }
}

void handle_uart_data_task(void *pvParameters) {
    DEBUG("handle_uart_data_task started");
    EspPicoCommHandler *uartCommHandler = (EspPicoCommHandler *)pvParameters;
    UartReceivedData receivedData;
    int retries = 0;

    std::string string;
    Message msg;

    while (true) {
        if (xQueueReceive(uartCommHandler->get_uart_received_data_queue_handle(), &receivedData, portMAX_DELAY) ==
            pdTRUE) {
            string = receivedData.buffer;
            if (convert_to_message(string, msg) == 0) {
                switch (msg.type) {
                    case MessageType::UNASSIGNED:
                        DEBUG("Unassigned message type received");
                        break;

                    case MessageType::RESPONSE:
                        if (msg.content[0] == "1") {
                            uartCommHandler->disable_response_wait_timer();
                        } else {
                            DEBUG("Response returned false");
                        }
                        break;

                    case MessageType::DATETIME:
                        if (msg.content[0] == "1") {
                            msg = datetime_response();
                            convert_to_string(msg, string);
                            uartCommHandler->send_data(string.c_str(), string.length());
                            while (uartCommHandler->set_response_wait_timer(string) == false && retries < RETRIES) {
                                vTaskDelay(1000 / portTICK_PERIOD_MS);
                                retries++;
                            }
                            string.clear();
                        } else {
                            DEBUG("Datetime request first value is not 1");
                        }
                        break;

                    case MessageType::ESP_INIT: // Should not be sent by Pico
                        DEBUG("ESP_INIT message sent by Pico");
                        break;

                    case MessageType::INSTRUCTIONS: // Should not be sent by Pico
                        DEBUG("INSTRUCTIONS message sent by Pico");
                        break;

                    case MessageType::PICTURE:
                        // TODO: TAKE PICTURE
                        // TODO: enqueue post req

                        // msg = response(true);
                        // convert_to_string(msg, string);
                        // uartCommHandler->send_data(string.c_str(), string.length());
                        // break;

                    case MessageType::DIAGNOSTICS:
                        // TODO: Gather diagnostics data
                        // TODO: enqueue post req

                        break;
                    default:
                        DEBUG("Unknown message type received");
                        break;
                }

                retries = 0;
            } else {
                // TODO: Handle failed conversion or something idk...
                DEBUG("Failed to convert received data to message");
            }
        }
    }
}

// Test task
void take_picture_and_save_to_sdcard_in_loop_task(void *pvParameters) {
    Camera *cameraPtr = (Camera *)pvParameters;
    std::string filepath;

    while (true) {
        cameraPtr->create_image_filename(filepath);
        cameraPtr->take_picture_and_save_to_sdcard(filepath.c_str());

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
