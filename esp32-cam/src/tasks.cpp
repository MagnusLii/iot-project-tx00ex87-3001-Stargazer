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

#define RECONNECT_TIMER_PERIOD 60000

// Enables monitoring of idle tasks on both cores
#define WATCHDOG_MONITOR_IDLE_TASKS

// Enables clearing the SD card when space is low
#define ENABLE_CLEARING_SD_CARD

// #define SAVE_TEST_SETTINGS_TO_SDCARD

// #define PRINT_SETTINGS_READ_FROM_SDCARD

// #define USE_TLS

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

// Timer stops itself if time is synced
void get_timestamp_timer_callback(TimerHandle_t timer) {
    RequestHandler *requestHandler = (RequestHandler *)pvTimerGetTimerID(timer);

    if (requestHandler->getTimeSyncedStatus() == true) {
        xTimerStop(timer, 0);
        return;
    }

    xQueueSend(requestHandler->getWebSrvRequestQueue(), requestHandler->getTimestampGETRequestptr(), 0);
    DEBUG("Timestamp GET request sent");

    xTimerStart(timer, pdMS_TO_TICKS(GET_REQUEST_TIMER_PERIOD));
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

bool read_file_base64_and_send(SDcardHandler *sdcardHandler, RequestHandler *requesthandler,
                               const std::string &filename, const int64_t image_id, QueueMessage *response) {
    size_t file_size = 300000;
    unsigned char *file_data =
        static_cast<unsigned char *>(heap_caps_malloc(file_size * sizeof(unsigned char), MALLOC_CAP_SPIRAM));

    int data_len = sdcardHandler->read_file_base64(filename.c_str(), file_data, file_size);
    if (data_len < 0) {
        DEBUG("Failed to read file");
        free(file_data);
        return false;
    }

    data_len = requesthandler->createImagePOSTRequest(file_data, file_size, data_len, image_id);
    if (data_len < 0) {
        DEBUG("Failed to create POST request");
        free(file_data);
        return false;
    }

#ifdef USE_TLS
    if (requesthandler->sendRequestTLS(file_data, data_len, response) != RequestHandlerReturnCode::SUCCESS) {
        DEBUG("Failed to send request");
    }
#else
    if (requesthandler->sendRequest(file_data, data_len, response) != RequestHandlerReturnCode::SUCCESS) {
        DEBUG("Failed to send request");
    }
#endif

    free(file_data);
    return true;
}

// Timer restarts itself if connection is still not established
void wifi_reconnect_timer_callback(TimerHandle_t timer) {
    WirelessHandler *wirelessHandler = (WirelessHandler *)pvTimerGetTimerID(timer);
    wirelessHandler->connect(wirelessHandler->get_setting(Settings::WIFI_SSID),
                             wirelessHandler->get_setting(Settings::WIFI_PASSWORD));

    if (wirelessHandler->isConnected() == false) { xTimerStart(timer, pdMS_TO_TICKS(RECONNECT_TIMER_PERIOD)); }
}

// ----------------------------------------------------------
// ------------------------TASKS-----------------------------
// ----------------------------------------------------------

void init_task(void *pvParameters) {
    auto handlers = std::make_shared<Handlers>();
    handlers->espPicoCommHandler = std::make_shared<EspPicoCommHandler>(UART_NUM_0);

    // Initialize sdcardHandler
    Sd_card_mount_settings sd_card_settings;
    handlers->sdcardHandler = std::make_shared<SDcardHandler>(sd_card_settings);
    if (handlers->sdcardHandler->get_sd_card_status() != ESP_OK) {
        DEBUG("Failed to initialize SD card");
        esp_restart();
    }

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
#endif // ENABLE_CLEARING_SD_CARD

    handlers->wirelessHandler = std::make_shared<WirelessHandler>(handlers->sdcardHandler);

    // Read settings from SD card
    std::unordered_map<Settings, std::string> settings;

#ifdef SAVE_TEST_SETTINGS_TO_SDCARD
    settings[Settings::WIFI_SSID] = TEST_WIFI_SSID;
    settings[Settings::WIFI_PASSWORD] = TEST_WIFI_PASSWORD;
    settings[Settings::WEB_DOMAIN] = TEST_WEB_SERVER;
    settings[Settings::WEB_PORT] = TEST_WEB_PORT;
    settings[Settings::WEB_TOKEN] = TEST_WEB_TOKEN;
    settings[Settings::WEB_CERTIFICATE] = TEST_CERTIFICATE;

    if (handlers->wirelessHandler->save_settings_to_sdcard(settings) != 0) {
        DEBUG("Failed to save settings to SD card");
        esp_restart();
    } else {
        DEBUG("Settings saved to SD card");
    }
#endif // SAVE_TEST_SETTINGS_TO_SDCARD

    if (handlers->sdcardHandler->read_all_settings(settings) != 0) {
        DEBUG("Failed to read settings from SD card");

        DEBUG("Setting settings to default values");
        settings[Settings::WIFI_SSID] = "DEFAULT_WIFI_SSID";
        settings[Settings::WIFI_PASSWORD] = "DEFAULT_WIFI_PASSWORD";
        settings[Settings::WEB_DOMAIN] = "DEFAULT_WEB_SERVER";
        settings[Settings::WEB_PORT] = "DEFAULT_WEB_PORT";
        settings[Settings::WEB_TOKEN] = "DEFAULT_WEB_TOKEN";
    }

    // Set settings in wirelessHandler
    handlers->wirelessHandler->set_all_settings(settings);
    DEBUG("Settings read from SD card");

#ifdef PRINT_SETTINGS_READ_FROM_SDCARD
    for (auto const &setting : settings) {
        DEBUG("Setting: ", setting.first, " Value: ", setting.second);
    }
#endif // PRINT_SETTINGS_READ_FROM_SDCARD

    // Initialize Wi-Fi
    if (handlers->wirelessHandler->init() != ESP_OK) {
        DEBUG("Failed to initialize Wi-Fi");
        esp_restart();
    }

    // Attempt to connect to wifi
    handlers->wirelessHandler->connect(handlers->wirelessHandler->get_setting(Settings::WIFI_SSID),
                                       handlers->wirelessHandler->get_setting(Settings::WIFI_PASSWORD));

    // Initialize request handler
    handlers->requestHandler = std::make_shared<RequestHandler>(handlers->wirelessHandler, handlers->sdcardHandler);

    // Initialize camera
    auto cameraHandler =
        std::make_shared<CameraHandler>(handlers->sdcardHandler, handlers->requestHandler->getWebSrvRequestQueue());
    handlers->cameraHandler = cameraHandler;

    // Create Timers
    TimerHandle_t getRequestTmr = xTimerCreate("GETRequestTimer", pdMS_TO_TICKS(GET_REQUEST_TIMER_PERIOD), pdTRUE,
                                               handlers->requestHandler.get(), get_request_timer_callback);
    TimerHandle_t getTimestampTmr = xTimerCreate("GETTimestampTimer", pdMS_TO_TICKS(20000), pdFALSE,
                                                 handlers->requestHandler.get(), get_timestamp_timer_callback);

    //  Start timers
    xTimerStart(getRequestTmr, GET_REQUEST_TIMER_PERIOD);
    xTimerStart(getTimestampTmr, 0);

    // Create tasks
    xTaskCreate(send_request_to_websrv_task, "send_request_to_websrv_task", 40960, handlers.get(), TaskPriorities::HIGH,
                nullptr);
    xTaskCreate(uart_read_task, "uart_read_task", 4096, handlers.get(), TaskPriorities::ABSOLUTE, nullptr);
    xTaskCreate(handle_uart_data_task, "handle_uart_data_task", 8192, handlers.get(), TaskPriorities::MEDIUM, nullptr);

    // Send ESP initialized message to Pico
    msg::Message msg = msg::device_status(true);
    std::string msg_str;
    convert_to_string(msg, msg_str);
    handlers->espPicoCommHandler->espInitMsgSent = true;
    handlers->espPicoCommHandler->send_msg_and_wait_for_response(msg_str.c_str(), msg_str.length());

    // Start reconnect loop if not connected
    if (handlers->wirelessHandler->isConnected() == false) {
        DEBUG("Failed to connect to Wi-Fi network");

        TimerHandle_t reconnect_timer =
            xTimerCreate("wifi_reconnect_timer", pdMS_TO_TICKS(RECONNECT_TIMER_PERIOD), pdFALSE,
                         handlers->wirelessHandler.get(), wifi_reconnect_timer_callback);
        xTimerStart(reconnect_timer, 0);
    }

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
    std::unordered_map<std::string, std::string> parsed_results;
    std::string string;

    // Used for sending instructions to the Pico
    msg::Message uart_msg;
    std::string uart_msg_str;

    SDcardHandler *sdcardHandler = handlers->sdcardHandler.get();
    // std::string file_data;

    while (true) {
        // Wait for a request to be available
        if (xQueueReceive(requestHandler->getWebSrvRequestQueue(), &request, portMAX_DELAY) == pdTRUE) {

            while (handlers->wirelessHandler->isConnected() == false) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            switch (request.requestType) {
                case RequestType::UNDEFINED:
                    DEBUG("Undefined request received");
                    break;

                case RequestType::GET_COMMANDS:
                    DEBUG("GET_COMMANDS request received");
#ifdef USE_TLS
                    requestHandler->sendRequestTLS(request, &response);
#else
                    requestHandler->sendRequest(request, &response);
#endif
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

                    read_file_base64_and_send(sdcardHandler, requestHandler, request.imageFilename, request.image_id,
                                              &response);

                    // Clear variables
                    response.buffer_length = 0;
                    response.str_buffer[0] = '\0';
                    break;

                case RequestType::POST:
                    DEBUG("Request type ", static_cast<int>(request.requestType), " received.");

#ifdef USE_TLS
                    requestHandler->sendRequestTLS(string, &response);
#else
                    requestHandler->sendRequest(request, &response);
#endif

                    if (requestHandler->parseHttpReturnCode(response.str_buffer) != 200) {
                        DEBUG("Request returned non-200 status code");
                        DEBUG("Request: ", string.c_str());
                        DEBUG("Response: ", response.str_buffer);
                    }
                    break;

                case RequestType::GET_TIME:
                    DEBUG("GET_TIME request received");

#ifdef USE_TLS
                    requestHandler->sendRequestTLS(request, &response);
#else
                    requestHandler->sendRequest(request, &response);
#endif

                    if (requestHandler->parseHttpReturnCode(response.str_buffer) != 200) {
                        DEBUG("Request returned non-200 status code");
                        DEBUG("Request: ", string.c_str());
                        DEBUG("Response: ", response.str_buffer);
                        break;
                    }

                    // Parse the response
                    if (sync_time(requestHandler->parseTimestamp(response.str_buffer)) !=
                        timeSyncLibReturnCodes::SUCCESS) {
                        DEBUG("Failed to sync time");
                        break;
                    }

                    // Set time synced status
                    requestHandler->setTimeSyncedStatus(true);

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
                            DEBUG("Waiting for response");
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

    WirelessHandler *wirelessHandler = handlers->wirelessHandler.get();

    RequestHandler *requestHandler = handlers->requestHandler.get();

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
                        DEBUG("Datetime request received");

                        // check if time is synced
                        if (handlers->requestHandler->getTimeSyncedStatus() == false) {
                            DEBUG("Time not synced, cannot respond to datetime request");

                            espPicoCommHandler->send_ACK_msg(false);
                            string.clear();
                            break;
                        }

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

                    case msg::MessageType::DEVICE_STATUS:
                        DEBUG("INIT message received");

                        if (espPicoCommHandler->espInitMsgSent == false) {
                            // Send own status and wait for confirmation
                            msg = msg::device_status(true);
                            convert_to_string(msg, string);
                            if (espPicoCommHandler->send_msg_and_wait_for_response(string.c_str(), string.length()) !=
                                0) {
                                DEBUG("Failed to send device status message");
                                break;
                            }
                        } else {
                            // ack otherwise
                            espPicoCommHandler->send_ACK_msg(true);
                        }

                        espPicoCommHandler->espInitMsgSent = false;

                        // send diagnostics message
                        handlers->requestHandler->createGenericPOSTRequest(&string, "/api/diagnostics", "status",
                                                                           std::stoi(msg.content[0]), "message",
                                                                           "Pico init status received");

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
                        // Send confirmation message
                        espPicoCommHandler->send_ACK_msg(true);

                        request.requestType = RequestType::POST;
                        if (std::stoi(msg.content[2]) <= 0) {
                            handlers->requestHandler->createGenericPOSTRequest(
                                &string, "/api/command", "token",
                                handlers->wirelessHandler->get_setting(Settings::WEB_TOKEN), "id",
                                std::stoi(msg.content[0].c_str()), "status", std::stoi(msg.content[1].c_str()));
                        } else {
                            handlers->requestHandler->createGenericPOSTRequest(
                                &string, "/api/command", "token",
                                handlers->wirelessHandler->get_setting(Settings::WEB_TOKEN), "id",
                                std::stoi(msg.content[0].c_str()), "status", std::stoi(msg.content[1].c_str()), "time",
                                std::stoi(msg.content[2].c_str()));
                        }

                        DEBUG("Command status message: ", string.c_str());

                        request.buffer_length = string.size();
                        strncpy(request.str_buffer, string.c_str(), string.size());
                        request.str_buffer[string.size()] = '\0';

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

                        vTaskDelay(pdMS_TO_TICKS(10000)); // The camera has a delay

                        // Convert UartReceivedData to QueueMessage
                        msg::convert_to_message(string, msg);

                        // Take picture and save to sd card
                        cameraHandler->create_image_filename(filepath);
                        if (cameraHandler->take_picture_and_save_to_sdcard(filepath.c_str()) != 0) {
                            DEBUG("Failed to take picture and save to SD card");
                            break;
                        }

                        // Send confirmation message
                        espPicoCommHandler->send_ACK_msg(true);

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

                        request.image_id = std::stoi(msg.content[0]);
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
                        handlers->requestHandler->createGenericPOSTRequest(
                            &string, "/api/diagnostics", "token", wirelessHandler->get_setting(Settings::WEB_TOKEN),
                            "status", std::stoi(msg.content[0].c_str()), "message", msg.content[1].c_str());

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
                        // Send confirmation message
                        espPicoCommHandler->send_ACK_msg(true);

                        // Update Wi-Fi settings
                        wirelessHandler->set_setting(msg.content[0].c_str(), msg.content[0].size(),
                                                     Settings::WIFI_SSID);
                        wirelessHandler->set_setting(msg.content[1].c_str(), msg.content[1].size(),
                                                     Settings::WIFI_PASSWORD);

                        if (wirelessHandler->save_settings_to_sdcard(*wirelessHandler->get_all_settings_pointer()) !=
                            0) {
                            DEBUG("Failed to save Wi-Fi settings to SD card");
                        }

                        // Attempt to reconnect to Wi-Fi
                        wirelessHandler->connect(wirelessHandler->get_setting(Settings::WIFI_SSID),
                                                 wirelessHandler->get_setting(Settings::WIFI_PASSWORD));
                        break;

                    case msg::MessageType::SERVER:
                        // Send confirmation message
                        espPicoCommHandler->send_ACK_msg(true);

                        // Update server settings
                        wirelessHandler->set_setting(msg.content[0].c_str(), msg.content[0].size(),
                                                     Settings::WEB_DOMAIN);
                        wirelessHandler->set_setting(msg.content[1].c_str(), msg.content[1].size(), Settings::WEB_PORT);

                        if (wirelessHandler->save_settings_to_sdcard(*wirelessHandler->get_all_settings_pointer()) !=
                            0) {
                            DEBUG("Failed to save server settings to SD card");
                        }

                        requestHandler->updateUserInstructionsGETRequest();

                        break;

                    case msg::MessageType::API:
                        // Send confirmation message
                        espPicoCommHandler->send_ACK_msg(true);

                        // Update API token
                        wirelessHandler->set_setting(msg.content[0].c_str(), msg.content[0].size(),
                                                     Settings::WEB_TOKEN);

                        if (wirelessHandler->save_settings_to_sdcard(*wirelessHandler->get_all_settings_pointer()) !=
                            0) {
                            DEBUG("Failed to save API token to SD card");
                        }

                        requestHandler->updateUserInstructionsGETRequest();

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
