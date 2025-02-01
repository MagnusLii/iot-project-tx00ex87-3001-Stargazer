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
#include <esp_task_wdt.h>
#include <map>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "testMacros.hpp"

struct Handlers {
    std::shared_ptr<WirelessHandler> wirelessHandler;
    std::shared_ptr<SDcardHandler> sdcardHandler;
    std::shared_ptr<RequestHandler> requestHandler;
    std::shared_ptr<EspPicoCommHandler> espPicoCommHandler;
    std::shared_ptr<CameraHandler> cameraHandler;
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
void get_request_timer_callback(TimerHandle_t timer) {
    RequestHandler *requestHandler = (RequestHandler *)pvTimerGetTimerID(timer);
    xQueueSend(requestHandler->getWebSrvRequestQueue(), requestHandler->getUserInstructionsGETRequestptr(), 0);
    DEBUG("GET request sent");
}

// does same as get_request_timer_callback but as a task
// TODO: remove if still unused during production
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
    int retries = 0;

    auto handlers = std::make_shared<Handlers>();

    EspPicoCommHandler espPicoCommHandler(UART_NUM_0);
    WirelessHandler wirelessHandler;
    handlers->wirelessHandler = std::make_shared<WirelessHandler>();
    handlers->espPicoCommHandler = std::make_shared<EspPicoCommHandler>(UART_NUM_0);

    // Initialize sdcardHandler
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    while (sdcardHandler->get_sd_card_status() != ESP_OK) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        sdcardHandler->mount_sd_card("/sdcard");
    }

    handlers->sdcardHandler = sdcardHandler;

    // Initialize Wi-Fi
    handlers->wirelessHandler->connect(WIFI_SSID, WIFI_PASSWORD);
    while (!handlers->wirelessHandler->isConnected()) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        handlers->wirelessHandler->disconnect();
        handlers->wirelessHandler->connect(WIFI_SSID, WIFI_PASSWORD);
    }
    DEBUG("Connected to Wi-Fi");

    // Initialize request handler
    auto requestHandler =
        std::make_shared<RequestHandler>(WEB_SERVER, WEB_PORT, WEB_TOKEN, handlers->wirelessHandler, handlers->sdcardHandler);
    handlers->requestHandler = requestHandler;

    // Initialize camera
    auto cameraHandler = std::make_shared<CameraHandler>(sdcardHandler, requestHandler->getWebSrvRequestQueue());
    handlers->cameraHandler = cameraHandler;

    // Set timezone
    set_tz(); // TODO: modify to set provided tz, and add verification to check if tz is set.

    // Create a timer to fetch user instructions from the web server
    TimerHandle_t getRequestTimer = xTimerCreate("GETRequestTimer", pdMS_TO_TICKS(GET_REQUEST_TIMER_PERIOD), pdTRUE,
                                                 requestHandler.get(), get_request_timer_callback);
    while (getRequestTimer == NULL && retries < RETRIES) {
        getRequestTimer = xTimerCreate("GETRequestTimer", pdMS_TO_TICKS(GET_REQUEST_TIMER_PERIOD), pdTRUE,
                                       requestHandler.get(), get_request_timer_callback);
        retries++;
    }
    if (getRequestTimer != NULL) {
        xTimerStart(getRequestTimer, 0);
    } else {
        DEBUG("Failed to create GET request timer");
    }

    // Create tasks
    xTaskCreate(send_request_to_websrv_task, "send_request_to_websrv_task", 8192, handlers.get(), TaskPriorities::HIGH,
                nullptr);
    xTaskCreate(uart_read_task, "uart_read_task", 4096, handlers.get(), TaskPriorities::ABSOLUTE, nullptr);
    xTaskCreate(handle_uart_data_task, "handle_uart_data_task", 4096, handlers.get(), TaskPriorities::MEDIUM, nullptr);

    DEBUG("Initialization complete. Deleting init task.");

    // Send ESP initialized message to Pico
    msg::Message msg = msg::esp_init(true);
    std::string msg_str;
    convert_to_string(msg, msg_str);
    espPicoCommHandler.send_data(msg_str.c_str(), msg_str.length());

    vTaskDelete(NULL);
}

// Sends requests to the web server
void send_request_to_websrv_task(void *pvParameters) {
    DEBUG("send_request_to_websrv_task started");

    Handlers *handlers = (Handlers *)pvParameters;

    RequestHandler *requestHandler = handlers->requestHandler.get();
    EspPicoCommHandler *espPicoCommHandler = handlers->espPicoCommHandler.get();

    // Used for server communication
    QueueMessage request;
    QueueMessage response;
    std::map<std::string, std::string> parsed_results;
    std::string response_str;

    // Used for sending instructions to the Pico
    msg::Message uart_msg;
    std::string uart_msg_str;

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
                    response_str = response.str_buffer;
                    if (JsonParser::parse(response_str, &parsed_results) != 0) {
                        DEBUG("Failed to parse response");
                        break;
                    }

                    // Verify correct keys.
                    if (parsed_results.find("target") == parsed_results.end() ||
                        parsed_results.find("id") == parsed_results.end() ||
                        parsed_results.find("position") == parsed_results.end()) {
                        DEBUG("Invalid response received");
                        break;
                    }

                    // create instructions uart message
                    uart_msg =
                        msg::instructions(parsed_results["target"], parsed_results["id"], parsed_results["position"]);
                    convert_to_string(uart_msg, uart_msg_str);

                    // Send the message to the Pico
                    espPicoCommHandler->send_data(uart_msg_str.c_str(), uart_msg_str.length());

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
    Handlers *handlers = (Handlers *)pvParameters;
    EspPicoCommHandler *espPicoCommHandler = handlers->espPicoCommHandler.get();
    uart_event_t event;

    UartReceivedData uartReceivedData;
    int retries = 0;
    std::string string;
    msg::Message msg;

    while (true) {
        if (xQueueReceive(espPicoCommHandler->get_uart_event_queue_handle(), (void *)&event, portMAX_DELAY)) {

            switch (event.type) {
                case UART_DATA:
                    uartReceivedData.len = uart_read_bytes(espPicoCommHandler->get_uart_num(), (uint8_t *)uartReceivedData.buffer,
                                                       sizeof(uartReceivedData.buffer), pdMS_TO_TICKS(10));
                    uartReceivedData.buffer[uartReceivedData.len] = '\0'; // Null-terminate the received string

                    if (espPicoCommHandler->get_waiting_for_response()) {
                        espPicoCommHandler->check_if_confirmation_msg(uartReceivedData);
                    } else {
                        while (xQueueSend(espPicoCommHandler->get_uart_received_data_queue_handle(), &uartReceivedData, 0) !=
                                   pdTRUE &&
                               retries < RETRIES) {
                            DEBUG("Failed to enqueue received data, retrying...");
                            retries++;
                        }
                        if (retries >= RETRIES) {
                            DEBUG("Failed to enqueue received data after ", RETRIES, " attempts");
                        }
                        retries = 0;
                    }
                    break;

                default:
                    DEBUG("Unknown event type received");
                    uart_flush_input(espPicoCommHandler->get_uart_num());
                    xQueueReset(espPicoCommHandler->get_uart_event_queue_handle());
                    break;
            }
        }
    }
}

void handle_uart_data_task(void *pvParameters) {
    DEBUG("handle_uart_data_task started");
    Handlers *handlers = (Handlers *)pvParameters;
    EspPicoCommHandler *espPicoCommHandler = handlers->espPicoCommHandler.get();
    UartReceivedData uartReceivedData;
    CameraHandler *cameraHandler = handlers->cameraHandler.get();

    std::string string;
    msg::Message msg;

    while (true) {
        if (xQueueReceive(espPicoCommHandler->get_uart_received_data_queue_handle(), &uartReceivedData, portMAX_DELAY) ==
            pdTRUE) {
            string = uartReceivedData.buffer;
            if (msg::convert_to_message(string, msg) == 0) {
                switch (msg.type) {
                    case msg::MessageType::UNASSIGNED:
                        DEBUG("Unassigned message type received");
                        break;

                    case msg::MessageType::RESPONSE:
                        DEBUG("Response message not filtered before reaching handle_uart_data_task");
                        break;

                    case msg::MessageType::DATETIME:
                        if (msg.content[0] == "1") {
                            msg = msg::datetime_response(get_datetime());
                            convert_to_string(msg, string);
                            espPicoCommHandler->send_data(string.c_str(), string.length());

                            // Waiting for confirmation response
                            espPicoCommHandler->send_msg_and_wait_for_response(string.c_str(), string.length());
                            string.clear();
                        } else {
                            DEBUG("Datetime request first value is not 1");
                        }
                        break;

                    case msg::MessageType::ESP_INIT: // Should not be sent by Pico
                        DEBUG("ESP_INIT message sent by Pico");
                        break;

                    case msg::MessageType::INSTRUCTIONS: // Should not be sent by Pico
                        DEBUG("INSTRUCTIONS message sent by Pico");
                        break;

                    case msg::MessageType::PICTURE:
                        // TODO: TAKE PICTURE


                        // TODO: enqueue post req

                        // msg = response(true);
                        // convert_to_string(msg, string);
                        // espPicoCommHandler->send_data(string.c_str(), string.length());
                        // break;

                    case msg::MessageType::DIAGNOSTICS:
                        // TODO: Gather diagnostics data
                        // TODO: enqueue post req

                        break;
                    default:
                        DEBUG("Unknown message type received");
                        break;
                }

            } else {
                // TODO: Handle failed conversion or something idk...
                DEBUG("Failed to convert received data to message");
            }
        }
    }
}

// Test task
void take_picture_and_save_to_sdcard_in_loop_task(void *pvParameters) {
    CameraHandler *cameraHandler = (CameraHandler *)pvParameters;
    std::string filepath;

    while (true) {
        cameraHandler->create_image_filename(filepath);
        cameraHandler->take_picture_and_save_to_sdcard(filepath.c_str());

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}