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

#include "testMacros.hpp"

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
void get_request_timer_callback(TimerHandle_t timer) {
    RequestHandler *requestHandler = (RequestHandler *)pvTimerGetTimerID(timer);
    xQueueSend(requestHandler->getWebSrvRequestQueue(), requestHandler->getUserInstructionsGETRequestptr(), 0);
    DEBUG("GET request sent");
}

// ----------------------------------------------------------
// ------------------------TASKS-----------------------------
// ----------------------------------------------------------

void init_task(void *pvParameters) {
    WirelessHandler wifi;
    SDcard sdcard("/sdcard");
    while (sdcard.get_sd_card_status() != ESP_OK) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        sdcard.mount_sd_card("/sdcard");
    }

    wifi.connect(WIFI_SSID, WIFI_PASSWORD);
    do {
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Give the wifi some time to connect
        if (!wifi.isConnected()) {
            wifi.disconnect();
            wifi.connect(WIFI_SSID, WIFI_PASSWORD);
        }
    } while (!wifi.isConnected());

    RequestHandler requestHandler(WEB_SERVER, WEB_PORT, WEB_TOKEN, std::make_shared<WirelessHandler>(wifi),
                                  std::make_shared<SDcard>(sdcard));

    Camera cameraPtr(std::make_shared<SDcard>(sdcard), requestHandler.getWebSrvRequestQueue());

    EspPicoCommHandler uartCommHandler(UART_NUM_0);

    // xTaskCreate(send_request_task, "send_request_task", 4096, &requestHandler, TaskPriorities::HIGH, nullptr);
    // xTimerCreate("get_request_timer", pdMS_TO_TICKS(30000), pdTRUE, &requestHandler, get_request_timer_callback);

    xTaskCreate(uart_read_task, "uart_read_task", 4096, &uartCommHandler, TaskPriorities::ABSOLUTE, nullptr);
    xTaskCreate(handle_uart_data_task, "handle_uart_data_task", 4096, &uartCommHandler, TaskPriorities::HIGH, nullptr);

    // xTaskCreate(take_picture_and_save_to_sdcard_in_loop_task, "take_picture_and_save_to_sdcard_in_loop_task", 4096,
    //             (void *)&cameraPtr, TaskPriorities::HIGH, NULL);

    vTaskDelete(NULL);
}

void send_request_task(void *pvParameters) {
    RequestHandler *requestHandler = (RequestHandler *)pvParameters;
    QueueMessage *request = nullptr;

    while (true) {
        // Wait for a request to be available
        if (xQueueReceive(requestHandler->getWebSrvRequestQueue(), &request, portMAX_DELAY) == pdTRUE) {
            switch (request->requestType) {
                case RequestType::GET_COMMANDS:
                    DEBUG("GET_COMMANDS request received");
                    requestHandler->sendRequest(*request);
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
void uart_read_task(void *pvParameters) {
    EspPicoCommHandler *uartCommHandler = (EspPicoCommHandler *)pvParameters;
    uart_event_t event;

    UartReceivedData receivedData;
    int retries = 0;

    Message msg = esp_init(true);
    std::string string;
    convert_to_string(msg, string);
    std::string read_data;

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
                           retries < ENQUEUE_RETRIES) {
                        DEBUG("Failed to enqueue received data, retrying...");
                        retries++;
                    }
                    if (retries >= ENQUEUE_RETRIES) {
                        DEBUG("Failed to enqueue received data after ", ENQUEUE_RETRIES, " attempts");
                    }
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
                            while (uartCommHandler->set_response_wait_timer(string) == false &&
                                   retries < ENQUEUE_RETRIES) {
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

                        msg = response(true);
                        convert_to_string(msg, string);
                        uartCommHandler->send_data(string.c_str(), string.length());
                        break;

                    case MessageType::DIAGNOSTICS:

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
