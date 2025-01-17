#include "sd-card.hpp"
#include "crc.hpp"
#include "debug.hpp"
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <string>

SDcard::SDcard(std::string mount_point_arg, int max_open_files, int CMD, int D0, int D1, int D2, int D3) {
    for (int i = 0; i < 3; i++) {
        this->sd_card_status = this->mount_sd_card(mount_point_arg, max_open_files, CMD, D0, D1, D2, D3);
        if (this->sd_card_status == ESP_OK) {
            DEBUG("SD card mounted at ", mount_point.c_str());
            this->file_mutex = xSemaphoreCreateMutex();
            return;
        }
        DEBUG("SD card mount failed, attempt ", i + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    DEBUG("Failed to mount SD card after retries, error: ", esp_err_to_name(sd_card_status));
    this->sd_card_status = ESP_FAIL;
    return;
}

SDcard::~SDcard() {
    if (this->sd_card_status == ESP_OK) {
        esp_vfs_fat_sdcard_unmount(mount_point.c_str(), NULL);
        DEBUG("SD card unmounted from ", mount_point.c_str());
    }

    if (this->file_mutex) { vSemaphoreDelete(this->file_mutex); }
}

esp_err_t SDcard::mount_sd_card(std::string mount_point_arg, int max_open_files, int CMD, int D0, int D1, int D2,
                                int D3) {

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {.format_if_mount_failed = false,
                                                     .max_files = max_open_files,
                                                     .allocation_unit_size = 16 * 1024,
                                                     .disk_status_check_enable = false,
                                                     .use_one_fat = false};
    sdmmc_card_t *card;

    const char *mount_point = mount_point_arg.c_str();
    this->mount_point = mount_point_arg;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode((gpio_num_t)CMD, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)D0, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)D1, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)D2, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)D3, GPIO_PULLUP_ONLY);

    return esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
}
std::string *SDcard::get_mount_point() { return &this->mount_point; }
const char *SDcard::get_mount_point_c_str() { return this->mount_point.c_str(); }
esp_err_t SDcard::get_sd_card_status() { return this->sd_card_status; }

// filename is expected to contain file extension
// Prepends mount_point to filename
int SDcard::write_file(const char *filename, std::string data) {
    if (xSemaphoreTake(file_mutex, portMAX_DELAY) != pdTRUE) { // Lock
        DEBUG("Failed to acquire mutex");
        return 1; // Mutex acquisition failure
    }

    std::string full_filename_str = this->mount_point + "/" + filename;

    DEBUG("Opening file ", full_filename_str.c_str());
    FILE *file = fopen(full_filename_str.c_str(), "w");

    if (!file) {
        DEBUG("Failed to open file for writing");
        perror("File open error");
        xSemaphoreGive(file_mutex);
        return 2; // File open failure
    }

    if (fwrite(data.c_str(), 1, data.length(), file) != data.length()) {
        DEBUG("File write failed");
        fclose(file);
        xSemaphoreGive(file_mutex);
        return 3; // File write failure
    }

    DEBUG("File write success");
    fclose(file);

    xSemaphoreGive(file_mutex);
    return 0; // Success
}

void SDcard::add_crc(std::string &data) {
    DEBUG("Adding CRC to data");
    DEBUG("Data: ", data.c_str());
    uint16_t crc = crc16(data);
    data += "," + std::to_string(crc);
}

bool SDcard::check_crc(const std::string &data) {
    std::string copy = data;
    size_t pos = copy.find_last_of(',');
    if (pos == std::string::npos) { return false; }
    std::string crc_str = copy.substr(pos + 1);
    uint16_t crc = std::stoi(crc_str);
    copy = copy.substr(0, pos);
    uint16_t crc_calc = crc16(copy);

    DEBUG("Data: ", data.c_str());
    DEBUG("Copy: ", copy.c_str());

    DEBUG("Extracted CRC: ", crc);
    DEBUG("Calculated CRC: ", crc_calc);
    return (crc == crc_calc);
}

int SDcard::save_all_settings(const std::vector<std::string> settings) {
    DEBUG("Saving all settings");
    std::string temp_settings;
    for (int i = 0; i < (int)Settings::CRC - 1; i++) {
        if (i >= settings.size()) {
            DEBUG("Insufficient settings provided");
            return 1; // Insufficient settings in vector
        }
        temp_settings += settings[i] + "\n";
    }

    add_crc(temp_settings);

    if (write_file("settings.txt", temp_settings)) {
        DEBUG("Failed to write settings to file");
        return 2; // File write failure
    }

    return 0; // Success
}

int SDcard::read_all_settings(std::vector<std::string> settings) {
    DEBUG("Reading all settings");
    std::string full_filename_str = this->mount_point + "/settings.txt";
    std::string temp_settings;
    DEBUG("Opening file: ", full_filename_str.c_str());
    FILE *fptr = fopen(full_filename_str.c_str(), "r");

    if (!fptr) {
        DEBUG("Failed to open file for reading");
        return 1; // File opening failure
    }

    char read_buffer[100];

    for (int i = 0; i < (int)Settings::CRC; i++) {
        if (!fgets(read_buffer, 100, fptr)) {
            DEBUG("Failed to read line from file");
            fclose(fptr);
            return 2; // File reading failure
        }
        read_buffer[strcspn(read_buffer, "\n")] = 0;
        temp_settings += read_buffer;
        temp_settings += "\n";
        DEBUG("Read: ", read_buffer);
    }

    if (!check_crc(temp_settings)) {
        DEBUG("CRC check failed");
        fclose(fptr);
        return 3; // CRC check failure
    }

    DEBUG("CRC check passed");

    size_t pos = 0;
    for (int i = 0; i < (int)Settings::CRC - 1; i++) {
        pos = temp_settings.find("\n");
        if (pos == std::string::npos) {
            DEBUG("Malformed settings data");
            fclose(fptr);
            return 4; // Malformed settings data
        }
        settings[i] = temp_settings.substr(0, pos);
        temp_settings.erase(0, pos + 1);
    }

    fclose(fptr);
    return 0; // Success
}

int SDcard::save_setting(const Settings settingID, const std::string &value) {
    std::vector<std::string> settings;
    if (read_all_settings(settings) != 0) {
        DEBUG("Failed to read settings");
        return 1; // Failed to read settings
    }

    settings[(int)settingID] = value;
    if (save_all_settings(settings) != 0) {
        DEBUG("Failed to save settings");
        return 2; // Failed to save settings
    }

    return 0; // Success
}

int SDcard::read_setting(const Settings settingID, std::string &value) {
    std::vector<std::string> settings;
    if (read_all_settings(settings) != 0) {
        DEBUG("Failed to read settings");
        return 1; // Failed to read settings
    }

    value = settings[(int)settingID];
    return 0; // Success
}
