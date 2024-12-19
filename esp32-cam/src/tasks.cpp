#include "tasks.hpp"
#include "debug.hpp"
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
#include "espPicoUartCommHandler.hpp"

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
    wifi.connect(WIFI_SSID, WIFI_PASSWORD);
    do {
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Give the wifi some time to connect
        if (!wifi.isConnected()) { wifi.disconnect(); }
    } while (!wifi.isConnected());
    RequestHandler requestHandler(WEB_SERVER, WEB_PORT, WEB_TOKEN, std::make_shared<WirelessHandler>(wifi),
                                  std::make_shared<SDcard>(sdcard));

    xTaskCreate(send_request_task, "send_request_task", 4096, &requestHandler, , nullptr);
    xTimerCreate("get_request_timer", pdMS_TO_TICKS(30000), pdTRUE, &requestHandler, get_request_timer_callback);
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

void uart_read_task(void *pvParameters) {
    EspPicoCommHandler *commHandler = (EspPicoCommHandler *)pvParameters;
    uart_event_t event;
    while (true) {
        if (xQueueReceive(commHandler->get_uart_event_queue(), (void*)&event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    char buffer[128];
                    int len = uart_read_bytes(commHandler->get_uart_num(), (uint8_t*)buffer, sizeof(buffer), portMAX_DELAY);
                    buffer[len] = '\0';  // Null-terminate the received string
                    // TODO: Process the received data
                    break;
                default:
                    DEBUG("Unknown event type received");
                    break;
            }
        }
    }
}
