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


// Used to reserve UART0 for Pico communication as they're the only free pins with no other critical functions.
// #define RESERVE_UART0_FOR_PICO_COMM

// Sends datetime response over UART0 every 10 seconds
// #define UART_DEMO

#define PRODUCTION_CODE

extern "C" {
void app_main(void);
}
void app_main(void) {
    DEBUG("Starting main");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    #ifdef RESERVE_UART0_FOR_PICO_COMM 
    esp_log_level_set("*", ESP_LOG_NONE);
    gpio_reset_pin(GPIO_NUM_1);
    gpio_reset_pin(GPIO_NUM_3);
    #endif

    #ifdef UART_DEMO
    EspPicoCommHandler uartCommHandler(UART_NUM_0);

    Message msg = datetime_response();
    std::string string;
    convert_to_string(msg, string);
    #endif

    #ifdef PRODUCTION_CODE
    xTaskCreate(init_task, "init-task", 8192,
                NULL, TaskPriorities::HIGH, NULL);
    #endif

    while (1) {
        // DEBUG("app_main endless catch");

        #ifdef UART_DEMO
        uartCommHandler.send_data(string.c_str(), string.length());
        #endif

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
