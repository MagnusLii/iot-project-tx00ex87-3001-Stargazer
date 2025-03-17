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

#include "mbedtls/platform.h"

#include <set>

extern "C" {
void app_main(void);
}
void app_main(void) {
    DEBUG("Starting main");
    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskCreate(init_task, "init-task", 8192, NULL, TaskPriorities::HIGH, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
