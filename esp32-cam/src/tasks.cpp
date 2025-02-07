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

#include "testMacros.hpp" // TODO: remove after testing

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

bool enqueue_with_retry(const QueueHandle_t queue, const void *item, TickType_t ticks_to_wait, int retries) {
    while (retries > 0) {
        if (xQueueSend(queue, item, ticks_to_wait) == pdTRUE) { return true; }
        retries--;
    }
    return false;
}

bool read_file_with_retry(SDcardHandler *sdcardHandler, const std::string &filename, std::string &file_data,
                          int retries) {
    while (retries > 0) {
        if (sdcardHandler->read_file(filename.c_str(), file_data) == 0) { return true; }
        retries--;
    }
    return false;
}

bool read_file_with_retry_base64(SDcardHandler *sdcardHandler, const std::string &filename, std::string &file_data,
                                 int retries) {
    while (retries > 0) {
        if (sdcardHandler->read_file_base64(filename.c_str(), file_data) == 0) { return true; }
        retries--;
    }
    return false;
}

// ----------------------------------------------------------
// ------------------------TASKS-----------------------------
// ----------------------------------------------------------

void init_task(void *pvParameters) {
    int retries = 0;

    auto handlers = std::make_shared<Handlers>();
    handlers->wirelessHandler = std::make_shared<WirelessHandler>();
    handlers->espPicoCommHandler = std::make_shared<EspPicoCommHandler>(UART_NUM_0);

    // Initialize sdcardHandler
    auto sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    while (sdcardHandler->get_sd_card_status() != ESP_OK) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        sdcardHandler->mount_sd_card("/sdcard");
    }
    handlers->sdcardHandler = sdcardHandler;
    DEBUG("SD card mounted");

    // Initialize camera
    auto cameraHandler =
        std::make_shared<CameraHandler>(sdcardHandler, handlers->requestHandler->getWebSrvRequestQueue());
    handlers->cameraHandler = cameraHandler;

    // TODO: clear the sd card if needed
    // TODO: read settings from sd card

    // Initialize Wi-Fi
    handlers->wirelessHandler->connect(WIFI_SSID, WIFI_PASSWORD);
    while (!handlers->wirelessHandler->isConnected() && retries < RETRIES) {
        DEBUG("Failed to connect to Wi-Fi. Retrying in 10 seconds...");
        vTaskDelay(pdMS_TO_TICKS(10000));
        handlers->wirelessHandler->disconnect();
        handlers->wirelessHandler->connect(WIFI_SSID, WIFI_PASSWORD);
    }
    retries = 0;

    if (!handlers->wirelessHandler->isConnected()) {
        // TODO: Request new settings from pico
    }

    DEBUG("Connected to Wi-Fi");

    // Initialize request handler
    handlers->requestHandler = std::make_shared<RequestHandler>(WEB_SERVER, WEB_PORT, WEB_TOKEN,
                                                                handlers->wirelessHandler, handlers->sdcardHandler);

    // Set timezone
    set_tz(); // TODO: modify to set provided tz, and add verification to check if tz is set.

    // Create a timer to fetch user instructions from the web server
    TimerHandle_t getRequestTimer = xTimerCreate("GETRequestTimer", pdMS_TO_TICKS(GET_REQUEST_TIMER_PERIOD), pdTRUE,
                                                 handlers->requestHandler.get(), get_request_timer_callback);
    while (getRequestTimer == NULL && retries < RETRIES) {
        getRequestTimer = xTimerCreate("GETRequestTimer", pdMS_TO_TICKS(GET_REQUEST_TIMER_PERIOD), pdTRUE,
                                       handlers->requestHandler.get(), get_request_timer_callback);
        retries++;
    }
    if (getRequestTimer != NULL) {
        xTimerStart(getRequestTimer, 0);
    } else {
        DEBUG("Failed to create GET request timer");
    }

    // Create tasks
    xTaskCreate(send_request_to_websrv_task, "send_request_to_websrv_task", 16384, handlers.get(), TaskPriorities::HIGH,
                nullptr);
    xTaskCreate(uart_read_task, "uart_read_task", 4096, handlers.get(), TaskPriorities::ABSOLUTE, nullptr);
    xTaskCreate(handle_uart_data_task, "handle_uart_data_task", 4096, handlers.get(), TaskPriorities::MEDIUM, nullptr);

    // Send ESP initialized message to Pico
    msg::Message msg = msg::esp_init(true);
    std::string msg_str;
    convert_to_string(msg, msg_str);
    handlers->espPicoCommHandler->send_data(msg_str.c_str(), msg_str.length());

    // Initialize watchdog for tasks
#ifdef WATCHDOG_MONITOR_IDLE_TASKS
    // Monitors idle tasks on both cores as we've multiple tasks that may be idle indefinetly as they wait for events.
    const esp_task_wdt_config_t twdt_config = {
        .timeout_ms = TASK_WATCHDOG_TIMEOUT,
        .idle_core_mask = (1 << 0) | (1 << 1), // Monitor core 0 and 1
        .trigger_panic = true                  // Trigger panic on timeout
    };

    esp_err_t err = esp_task_wdt_init(&twdt_config);
    if (err == ESP_OK) {
        printf("Task Watchdog initialized successfully for both cores.\n");
    } else {
        printf("Failed to initialize Task Watchdog: %d\n", err);
    }
#endif

    DEBUG("Initialization complete. Deleting init task.");
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
    std::string string;

    // Used for sending instructions to the Pico
    msg::Message uart_msg;
    std::string uart_msg_str;

    SDcardHandler *sdcardHandler = handlers->sdcardHandler.get();
    std::string file_data;

    // DEBUG variables
    int counter = 0;

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
                    string = response.str_buffer;
                    if (JsonParser::parse(string, &parsed_results) != 0) {
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

                    // read image from sd card in base64
                    if (read_file_with_retry_base64(sdcardHandler, request.imageFilename, file_data, RETRIES) ==
                        false) {
                        DEBUG("Failed to read image file");
                        break;
                    }
                    DEBUG("Image data read from file");

                    // Create a POST request
                    DEBUG("Creating POST request");
                    requestHandler->createImagePOSTRequest(&string, request.image_id, file_data);

                    // Send the request and enqueue the response
                    DEBUG("Sending POST request: ");

                    requestHandler->sendRequest(string, &response);

                    // Clear variables
                    response.buffer_length = 0;
                    response.str_buffer[0] = '\0';
                    file_data.clear();
                    string.clear();
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
    uart_event_t uart_event;

    char data_read_from_uart[UART_RING_BUFFER_SIZE];
    size_t uart_databuffer_len;
    UartReceivedData uartReceivedData;
    std::string string;
    msg::Message msg;

    int return_code;

    while (true) {
        if (xQueueReceive(espPicoCommHandler->get_uart_event_queue_handle(), (void *)&uart_event, portMAX_DELAY)) {

            switch (uart_event.type) {
                case UART_DATA:
                    uart_databuffer_len =
                        uart_read_bytes(espPicoCommHandler->get_uart_num(), (uint8_t *)data_read_from_uart,
                                        (sizeof(data_read_from_uart) - 1), pdMS_TO_TICKS(10));
                    if (uart_databuffer_len == -1) {
                        DEBUG("Failed to read data from UART");
                        break;
                    }
                    data_read_from_uart[uart_databuffer_len] = '\0'; // Null-terminate the received string

                    // Extract message from buffer
                    return_code =
                        extract_msg_from_uart_buffer(data_read_from_uart, &uart_databuffer_len, &uartReceivedData);
                    while (return_code == 0) {
                        // Check we're waiting for a response
                        if (espPicoCommHandler->get_waiting_for_response()) {
                            espPicoCommHandler->check_if_confirmation_msg(uartReceivedData);
                        }
                        // enqueue extracted message
                        else {
                            if (enqueue_with_retry(espPicoCommHandler->get_uart_received_data_queue_handle(),
                                                   &uartReceivedData, 0, RETRIES) == false) {
                                DEBUG("Failed to enqueue received data");
                            }
                        }
                        extract_msg_from_uart_buffer(data_read_from_uart, &uart_databuffer_len, &uartReceivedData);
                    }
                    break;

                default:
                    DEBUG("Unknown uart_event type received");
                    uart_flush_input(espPicoCommHandler->get_uart_num());
                    xQueueReset(espPicoCommHandler->get_uart_event_queue_handle());
                    break;
            }
        }
    }

    free(data_read_from_uart); // should never reach here
}

void handle_uart_data_task(void *pvParameters) {
    DEBUG("handle_uart_data_task started");
    Handlers *handlers = (Handlers *)pvParameters;
    EspPicoCommHandler *espPicoCommHandler = handlers->espPicoCommHandler.get();
    UartReceivedData uartReceivedData;

    CameraHandler *cameraHandler = handlers->cameraHandler.get();
    std::string filepath;
    QueueMessage request;

    std::string string;
    msg::Message msg = msg::response(true);

    std::string response_str;
    convert_to_string(msg, response_str);

    while (true) {
        if (xQueueReceive(espPicoCommHandler->get_uart_received_data_queue_handle(), &uartReceivedData,
                          portMAX_DELAY) == pdTRUE) {
            string = uartReceivedData.buffer;
            DEBUG("Received data: ", string.c_str());
            if (msg::convert_to_message(string, msg) == 0) {

                switch (msg.type) {
                    case msg::MessageType::UNASSIGNED:
                        DEBUG("Unassigned message type received");
                        string.clear();
                        break;

                    case msg::MessageType::RESPONSE:
                        DEBUG("Response message not filtered before reaching handle_uart_data_task");
                        string.clear();
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
                        string.clear();
                        break;

                    case msg::MessageType::INSTRUCTIONS: // Should not be sent by Pico
                        DEBUG("INSTRUCTIONS message sent by Pico");
                        string.clear();
                        break;

                    case msg::MessageType::PICTURE:
                        // Send confirmation message
                        // TODO: maybe handle confirmation message in uart_read_task
                        espPicoCommHandler->send_data(response_str.c_str(), response_str.length());

                        // Convert UartReceivedData to QueueMessage
                        msg::convert_to_message(string, msg);

                        // Take picture and save to sd card
                        cameraHandler->create_image_filename(filepath);
                        cameraHandler->take_picture_and_save_to_sdcard(filepath.c_str());

                        // Create queue message for enqueuing
                        request.requestType = RequestType::POST_IMAGE;
                        if (filepath.size() < BUFFER_SIZE) {
                            strncpy(request.imageFilename, filepath.c_str(), filepath.size());
                            request.imageFilename[filepath.size()] = '\0';
                            DEBUG("Image filename: ", request.imageFilename);
                        } else {
                            DEBUG("Filename too long");
                            break;
                        }

                        // store image/command id in request
                        if (msg.content.size() < 2) {
                            DEBUG("Invalid message content");
                            break;
                        }
                        request.image_id = std::stoi(msg.content[1]);
                        DEBUG("Image ID: ", request.image_id);

                        // Enqueue the request
                        if (enqueue_with_retry(handlers->requestHandler->getWebSrvRequestQueue(), &request, 0,
                                               RETRIES) == false) {
                            DEBUG("Failed to enqueue POST_IMAGE request");
                        }

                        // Clear variables
                        request.buffer_length = 0;
                        request.imageFilename[0] = '\0';
                        filepath.clear();
                        string.clear();
                        break;

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