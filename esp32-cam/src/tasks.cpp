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
    DEBUG("Userinstructions GET request sent");
}

void get_timestamp_timer_callback(TimerHandle_t timer) {
    RequestHandler *requestHandler = (RequestHandler *)pvTimerGetTimerID(timer);

    if (requestHandler->getTimeSyncedStatus() == true) {
        xTimerStop(timer, 0);
        return;
    }

    xQueueSend(requestHandler->getWebSrvRequestQueue(), requestHandler->getTimestampGETRequestptr(), 0);
    DEBUG("Timestamp GET request sent");

    xTimerStart(timer, pdMS_TO_TICKS(60000)); // Retry in 1 minute
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
    handlers->sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    if (handlers->sdcardHandler->get_sd_card_status() != ESP_OK) {
        DEBUG("Failed to initialize SD card");
        esp_restart();
    }

    // TODO: clear the sd card if needed
    uint32_t free_space = handlers->sdcardHandler->get_sdcard_free_space();
    if (free_space < 100000) {
        DEBUG("SD card storage getting low. Backup and clear the SD card.");
        // TODO: Add diagnostics?
    }
#ifdef ENABLE_CLEARING_SD_CARD
    else if (handlers->sdcardHandler->get_sdcard_free_space() < 10000) {
        DEBUG("SD card almost full. Clearing the SD card.");
        handlers->sdcardHandler->clear_sdcard();
        // TODO: Add diagnostics msg
    }
#endif

    // TODO: read settings from sd card

    // Initialize Wi-Fi
    handlers->wirelessHandler->connect(WIFI_SSID, WIFI_PASSWORD);
    while (!handlers->wirelessHandler->isConnected() && retries < RETRIES) {
        DEBUG("Failed to connect to Wi-Fi. Retrying in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        handlers->wirelessHandler->connect(WIFI_SSID, WIFI_PASSWORD);
        retries++;
    }
    retries = 0;

    if (!handlers->wirelessHandler->isConnected()) {
        // TODO: Request new settings from pico
    }

    DEBUG("Connected to Wi-Fi");

    // Initialize request handler
    handlers->requestHandler = std::make_shared<RequestHandler>(WEB_SERVER, WEB_PORT, WEB_TOKEN,
                                                                handlers->wirelessHandler, handlers->sdcardHandler);

    // Initialize camera
    auto cameraHandler =
        std::make_shared<CameraHandler>(handlers->sdcardHandler, handlers->requestHandler->getWebSrvRequestQueue());
    handlers->cameraHandler = cameraHandler;

    // Set timezone
    set_tz(); // TODO: modify to set provided tz, and add verification to check if tz is set.

    // Create Timers
    TimerHandle_t getRequestTmr = xTimerCreate("GETRequestTimer", pdMS_TO_TICKS(GET_REQUEST_TIMER_PERIOD), pdTRUE,
                                               handlers->requestHandler.get(), get_request_timer_callback);
    TimerHandle_t getTimestampTmr = xTimerCreate("GETTimestampTimer", pdMS_TO_TICKS(GET_REQUEST_TIMER_PERIOD), pdFALSE,
                                                 handlers->requestHandler.get(), get_timestamp_timer_callback);

    //  Start timers
    xTimerStart(getRequestTmr, GET_REQUEST_TIMER_PERIOD);
    xTimerStart(getTimestampTmr, 0);

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
        DEBUG("Task Watchdog initialized successfully for both cores.");
    } else {
        DEBUG("Failed to initialize Task Watchdog: ", err);
    }
#endif

    DEBUG("Initialization complete. Deleting init task.");
    vTaskDelete(NULL);
}

// Sends requests to the web server
// Requires RequestHandler, EspPicoCommHandler, and SDcardHandler to be initialized
void send_request_to_websrv_task(void *pvParameters) {
    DEBUG("send_request_to_websrv_task started");

    Handlers *handlers = (Handlers *)pvParameters;

    RequestHandler *requestHandler = handlers->requestHandler.get();
    EspPicoCommHandler *espPicoCommHandler = handlers->espPicoCommHandler.get();

    // Used for server communication
    QueueMessage request;
    QueueMessage response = {"\0", 0, "\0", 0, RequestType::UNDEFINED};
    std::map<std::string, std::string> parsed_results;
    std::string string;

    // Used for sending instructions to the Pico
    msg::Message uart_msg;
    std::string uart_msg_str;

    SDcardHandler *sdcardHandler = handlers->sdcardHandler.get();
    std::string file_data;

    while (true) {
        // Wait for a request to be available
        // TODO: add mutex
        if (xQueueReceive(requestHandler->getWebSrvRequestQueue(), &request, portMAX_DELAY) == pdTRUE) {
            switch (request.requestType) {
                case RequestType::UNDEFINED:
                    DEBUG("Undefined request received");
                    break;

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

                case RequestType::POST:
                    DEBUG("Request type ", request.requestType, " received.");
                    requestHandler->sendRequest(string, &response);

                    if (requestHandler->parseHttpReturnCode(response.str_buffer) != 200) {
                        DEBUG("Request returned non-200 status code");
                        DEBUG("Request: ", string.c_str());
                        DEBUG("Response: ", response.str_buffer);
                    }
                    break;

                case RequestType::GET_TIME:
                    DEBUG("GET_TIME request received");
                    requestHandler->sendRequest(request, &response);

                    if (requestHandler->parseHttpReturnCode(response.str_buffer) != 200) {
                        DEBUG("Request returned non-200 status code");
                        DEBUG("Request: ", string.c_str());
                        DEBUG("Response: ", response.str_buffer);
                        break;
                    }

                    // Parse the response
                    if (sync_time(requestHandler->parseTimestamp(response.str_buffer)) !=
                        timeSyncLibReturnCodes::SUCCESS) {
                        // TODO: add error handling
                        DEBUG("Failed to sync time");
                        break;
                    }

                    // Set timezone
                    set_tz();

                    // Set time synced status
                    requestHandler->setTimeSyncedStatus(true);

                    // TODO: forward to pico?
                    // uart_msg = msg::datetime_response(get_datetime());
                    // convert_to_string(uart_msg, uart_msg_str);
                    // espPicoCommHandler->send_data(uart_msg_str.c_str(), uart_msg_str.length());

                    // Clear variables
                    response.buffer_length = 0;
                    response.str_buffer[0] = '\0';
                    uart_msg_str.clear();
                    break;

                default:
                    // TODO: add error handling
                    DEBUG("Unknown request type received");
                    break;
            }
        }
    }
}

// Run on highest prio.
// Reads uart and enqueues received buffer to queue.
// Requires EspPicoCommHandler to be initialized
// TODO: holy shit this is trash plz have time to rework...
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
                                        (sizeof(data_read_from_uart) - 1), pdMS_TO_TICKS(100));
                    if (uart_databuffer_len == -1) {
                        DEBUG("Failed to read data from UART");
                        break;
                    }
                    data_read_from_uart[uart_databuffer_len] = '\0'; // Null-terminate the received string
                    DEBUG("Data read from UART: ", data_read_from_uart);
                    // Extract message from buffer
                    return_code =
                        extract_msg_from_uart_buffer(data_read_from_uart, &uart_databuffer_len, &uartReceivedData);
                    DEBUG("Return code: ", return_code);
                    while (return_code == 0) {
                        // Check we're waiting for a response
                        if (espPicoCommHandler->get_waiting_for_response()) {
                            espPicoCommHandler->check_if_confirmation_msg(uartReceivedData);
                        }
                        // enqueue extracted message
                        else {
                            DEBUG("Enqueuing : ", uartReceivedData.buffer);
                            if (enqueue_with_retry(espPicoCommHandler->get_uart_received_data_queue_handle(),
                                                   &uartReceivedData, 0, RETRIES) == false) {
                                DEBUG("Failed to enqueue received data");
                            }
                        }
                        return_code =
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

// Requires EspPicoCommHandler to be initialized
void handle_uart_data_task(void *pvParameters) {
    DEBUG("handle_uart_data_task started");
    Handlers *handlers = (Handlers *)pvParameters;
    EspPicoCommHandler *espPicoCommHandler = handlers->espPicoCommHandler.get();
    UartReceivedData uartReceivedData;

    CameraHandler *cameraHandler = handlers->cameraHandler.get();
    std::string filepath;
    QueueMessage request;

    std::string string;
    msg::Message msg;

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

                    case msg::MessageType::ESP_INIT:
                        DEBUG("ESP_INIT message received");
                        // send confirmation message
                        espPicoCommHandler->send_ACK_msg(true);

                        // send diagnostics message
                        handlers->requestHandler->createGenericPOSTRequest(&string, "/api/diagnostics", 2, "\"status\"",
                                                                           msg.content[0].c_str(), "\"message\"",
                                                                           "\"Pico init status received\"");

                        strncpy(request.str_buffer, string.c_str(), string.size());
                        request.str_buffer[string.size()] = '\0';
                        request.buffer_length = string.size();

                        DEBUG("Diagnostics message: ", request.str_buffer);

                        // Enqueue the request
                        if (enqueue_with_retry(handlers->requestHandler->getWebSrvRequestQueue(), &request, 0,
                                               RETRIES) == false) {
                            DEBUG("Failed to enqueue POST_IMAGE request");
                        }

                        // Clear variables
                        request.buffer_length = 0;
                        request.str_buffer[0] = '\0';
                        string.clear();
                        break;

                    case msg::MessageType::INSTRUCTIONS: // Should not be sent by Pico
                        DEBUG("INSTRUCTIONS message sent by Pico");
                        string.clear();
                        break;

                    case msg::MessageType::CMD_STATUS:
                        request.requestType = RequestType::POST;
                        handlers->requestHandler->createGenericPOSTRequest(&string, "/api/command", 4, "\"id\"",
                                                                           msg.content[0].c_str(), "\"status\"",
                                                                           msg.content[1].c_str());

                        DEBUG("Command status message: ", string.c_str());

                        if (enqueue_with_retry(handlers->requestHandler->getWebSrvRequestQueue(), &request, 0,
                                               RETRIES) == false) {
                            DEBUG("Failed to enqueue POST_IMAGE request");
                        }

                        // Clear variables
                        request.buffer_length = 0;
                        request.str_buffer[0] = '\0';
                        string.clear();

                        break;

                    case msg::MessageType::PICTURE:
                        // Send confirmation message
                        espPicoCommHandler->send_ACK_msg(true);

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

                        request.image_id = std::stoi(msg.content[0]); // TODO:change when message format is updated.
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
                        // Send confirmation message
                        espPicoCommHandler->send_ACK_msg(true);

                        // Create POST request.
                        request.requestType = RequestType::POST;
                        handlers->requestHandler->createGenericPOSTRequest(&string, "/api/diagnostics", 2, "\"status\"",
                                                                           msg.content[0].c_str(), "\"message\"",
                                                                           msg.content[1].c_str());

                        strncpy(request.str_buffer, string.c_str(), string.size());
                        request.str_buffer[string.size()] = '\0';
                        request.buffer_length = string.size();

                        DEBUG("Diagnostics message: ", request.str_buffer);

                        // Enqueue the request
                        if (enqueue_with_retry(handlers->requestHandler->getWebSrvRequestQueue(), &request, 0,
                                               RETRIES) == false) {
                            DEBUG("Failed to enqueue POST_IMAGE request");
                        }

                        // Clear variables
                        request.buffer_length = 0;
                        request.str_buffer[0] = '\0';
                        string.clear();
                        break;

                    case msg::MessageType::WIFI:
                        // TODO:
                        break;

                    case msg::MessageType::SERVER:
                        // TODO:
                        break;

                    case msg::MessageType::API:
                        // TODO:
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

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}