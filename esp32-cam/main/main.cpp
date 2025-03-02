// TESTING DEFINES

// Used to reserve UART0 for Pico communication as they're the only free pins with no other critical functions.
// #define RESERVE_UART0_FOR_PICO_COMM

// Sends datetime response over UART0 every 10 seconds
// #define UART_DEMO

// Settings file write and read test.
// #define WRITE_READ_SETTINGS_TEST

// Make uart msg string with CRC
// #define UART_MSG_CRC

// Generic POST request test
// #define GENERIC_POST_REQUEST_TEST

// PRODUCTION DEFINES

// Enables production code
#define PRODUCTION_CODE

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "TaskPriorities.hpp"
#include "camera.hpp"
#include "debug.hpp"
#include "requestHandler.hpp"
#include "sd-card.hpp"
#include "testMacros.hpp"
#include "wireless.hpp"
#include <memory>

#include "esp_sntp.h"
#include "tasks.hpp"
#include "timesync-lib.hpp"

#include "espPicoUartCommHandler.hpp"
#include "message.hpp"

#include "esp_task_wdt.h"

extern "C" {
void app_main(void);
}
void app_main(void) {
    DEBUG("Starting main");
    vTaskDelay(pdMS_TO_TICKS(1000));

#ifdef RESERVE_UART0_FOR_PICO_COMM
    DEBUG("Disabling DEBUGS, switching UART to pico comm mode");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow for debug messages to be sent before disabling them
    esp_log_level_set("*", ESP_LOG_NONE);
    gpio_reset_pin(GPIO_NUM_0);
    gpio_reset_pin(GPIO_NUM_3);
#endif

#ifdef UART_MSG_CRC
    // msg::Message msg = msg::response(true);
    // msg::Message msg = msg::datetime_request();
    // msg::Message msg = msg::datetime_response(1740828909);
    // msg::Message msg = msg::esp_init(1);
    // msg::Message msg = msg::instructions(2, 1, 1);
    // msg::Message msg = msg::cmd_status(1, 1, 1);
    // msg::Message msg = msg::picture(1, 1);
    // msg::Message msg = msg::diagnostics(1, "test");
    // msg::Message msg = msg::wifi("ssid", "password");
    // msg::Message msg = msg::server("127.0.0.1:8080");
    msg::Message msg = msg::api("token-asdfgsdfads-14vfds-245vsd");
    std::string str;

    msg::convert_to_string(msg, str);
    DEBUG("Message string: ", str.c_str());
    int return_code = msg::convert_to_message(str, msg);
    DEBUG("Return code: ", return_code);
#endif

#ifdef WRITE_READ_SETTINGS_TEST
    // Increase main task stack size to avoid stack overflow (instructions in README)
    int retries = 0;
    std::vector<std::string> settings = {"WIFI_SSID", "WIFI_PASSWORD", "WEB_PATH", "WEB_PORT"};
    SDcardHandler sdcardHandler("/sdcard");
    while (sdcardHandler.save_all_settings(settings) != 0 && retries < 3) {
        DEBUG("Retrying save settings");
        retries++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    retries = 0;

    while (sdcardHandler.read_all_settings(settings) != 0 && retries < 3) {
        DEBUG("Retrying read settings");
        retries++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

#ifdef UART_DEMO
#include "tasks.hpp"
    Handlers handlers;
    handlers.espPicoCommHandler = std::make_shared<EspPicoCommHandler>(UART_NUM_0);

    // msg::Message msg = msg::datetime_response();
    // std::string string;
    // convert_to_string(msg, string);

    xTaskCreate(uart_read_task, "uart_read_task", 4096, &handlers, TaskPriorities::MEDIUM, NULL);

#endif

#ifdef GENERIC_POST_REQUEST_TEST
    // Increase main task stack size to avoid stack overflow (instructions in README)
    std::shared_ptr<WirelessHandler> wirelessHandler = std::make_shared<WirelessHandler>();
    std::shared_ptr<SDcardHandler> sdcardHandler = std::make_shared<SDcardHandler>("/sdcard");
    RequestHandler requestHandler("webserver", "webport", "webtoken", wirelessHandler, sdcardHandler);

    std::string request;
    requestHandler.createGenericPOSTRequest(&request, "/api/command", 4, "command", "1", "value", "3");
    DEBUG("Request: ", request.c_str());
#endif

#ifdef PRODUCTION_CODE
    xTaskCreate(init_task, "init-task", 8192, NULL, TaskPriorities::HIGH, NULL);
#endif

    while (1) {
#ifdef UART_DEMO
        // espPicoCommHandler.send_data(string.c_str(), string.length());
#endif

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
