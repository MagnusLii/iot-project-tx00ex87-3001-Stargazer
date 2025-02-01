// Used to reserve UART0 for Pico communication as they're the only free pins with no other critical functions.
// #define RESERVE_UART0_FOR_PICO_COMM

// Sends datetime response over UART0 every 10 seconds
// #define UART_DEMO

// Settings file write and read test.
// #define WRITE_READ_SETTINGS_TEST

// Enables production code
#define PRODUCTION_CODE

// Enables monitoring of idle tasks on both cores
#define WATCHDOG_MONITOR_IDLE_TASKS

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

#include "debug-functions.hpp"

#include "espPicoUartCommHandler.hpp"
#include "message.hpp"

#include "esp_task_wdt.h"

extern "C" {
void app_main(void);
}
void app_main(void) {
    DEBUG("Starting main");
    vTaskDelay(pdMS_TO_TICKS(1000));

// Initialize watchdog for tasks
#ifdef WATCHDOG_MONITOR_IDLE_TASKS
    // Monitors idle tasks on both cores as we've multiple tasks that may be idle indefinetly as they wait for events.
    const esp_task_wdt_config_t twdt_config = {
        .timeout_ms = TASK_WATCHDOG_TIMEOUT,   // 5-second timeout
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

#ifdef WRITE_READ_SETTINGS_TEST
    int retries = 0;
    std::vector<std::string> settings = {"WIFI_SSID", "WIFI_PASSWORD", "WEB_PATH", "WEB_PORT"};
    SDcard sdcard("/sdcard");
    while (sdcard.save_all_settings(settings) != 0 && retries < 3) {
        DEBUG("Retrying save settings");
        retries++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    retries = 0;

    while (sdcard.read_all_settings(settings) != 0 && retries < 3) {
        DEBUG("Retrying read settings");
        retries++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

#ifdef UART_DEMO
    EspPicoCommHandler espPicoCommHandler(UART_NUM_0);

    Message msg = datetime_response();
    std::string string;
    convert_to_string(msg, string);

#endif

#ifdef PRODUCTION_CODE
    xTaskCreate(init_task, "init-task", 8192, NULL, TaskPriorities::HIGH, NULL);
#endif

    while (1) {
#ifdef UART_DEMO
        espPicoCommHandler.send_data(string.c_str(), string.length());
#endif

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
