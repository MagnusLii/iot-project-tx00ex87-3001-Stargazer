#define ENABLE_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#include "camera.hpp"
#include "sd-card.hpp"
#include <memory>
#include "TaskPriorities.hpp"


extern "C" {
    void app_main(void);
}
void app_main(void){
    xTaskCreate(take_picture_and_save_to_sdcard_in_loop_task, "take_picture_and_save_to_sdcard_in_loop_task", 4096, NULL, TaskPriorities::ABSOLUTE, NULL);
}