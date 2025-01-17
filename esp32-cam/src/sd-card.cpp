#include "sd-card.hpp"
#include "crc.hpp"
#include "debug.hpp"
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <string>

/**
 * @brief Initializes the SDcard object and attempts to mount the SD card.
 *
 * This constructor tries to mount the SD card at the specified mount point up to 3 times.
 * If successful, it creates a mutex for file operations. If the mount fails, an error message is logged
 * and sd_card_status is set to ESP_FAIL.
 *
 * @param mount_point_arg The mount point where the SD card should be mounted.
 * @param max_open_files The maximum number of files that can be open simultaneously.
 * @param CMD The command pin used for SD card communication.
 * @param D0, D1, D2, D3 Data pins used for SD card communication.
 *
 * @return None
 *
 * @note The function will log the SD card status on each attempt and will return after successful mounting or failure
 * after retries.
 */
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

/**
 * @brief Mounts the SD card at the specified mount point.
 *
 * This function configures and attempts to mount the SD card with the given parameters.
 * It sets up the necessary GPIO pins and mount configurations, and then tries to mount the SD card using the specified
 * settings.
 *
 * @param mount_point_arg The mount point where the SD card should be mounted.
 * @param max_open_files The maximum number of files that can be open simultaneously.
 * @param CMD The command pin used for SD card communication.
 * @param D0, D1, D2, D3 Data pins used for SD card communication.
 *
 * @return esp_err_t The result of the mounting attempt (ESP_OK on success, or an error code on failure).
 *
 * @note This function configures GPIO pull-up resistors for the SD card pins before attempting to mount.
 */
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

/**
 * @brief Returns a pointer to the mount point string.
 *
 * @return std::string* A pointer to the mount point string.
 */
std::string *SDcard::get_mount_point() { return &this->mount_point; }

/**
 * @brief Returns the mount point as a C-style string.
 *
 * @return const char* A constant pointer to the C-style string representing the mount point.
 */
const char *SDcard::get_mount_point_c_str() { return this->mount_point.c_str(); }

/**
 * @brief Returns the current status of the SD card.
 *
 * @return esp_err_t The current SD card status (e.g., ESP_OK or error code).
 */
esp_err_t SDcard::get_sd_card_status() { return this->sd_card_status; }

/**
 * @brief Writes data to a file on the SD card.
 *
 * This function prepends the mount point to the given filename, attempts to open the file for writing,
 * and writes the provided data to the file. Utilizes mutex for thread safety and returns an error code on failure.
 *
 * @param filename The name of the file to write to, including the file extension.
 * @param data The data to write to the file.
 *
 * @return int Returns 0 on success
 *  - 1: Mutex acquisition failure.
 *  - 2: File open failure.
 *  - 3: File write failure.
 *
 * @note The function locks a mutex to ensure that file operations are thread-safe.
 */
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

/**
 * @brief Adds a CRC value to the given data string.
 *
 * This function computes the CRC16 checksum of the provided data and appends it to the data string,
 * separated by a comma. The original data string is modified in place.
 *
 * @param data The data string to which the CRC will be appended.
 *
 * @return void This function does not return any value.
 *
 * @note The CRC16 checksum is calculated and appended to the data as a comma-separated value.
 */
void SDcard::add_crc(std::string &data) {
    DEBUG("Adding CRC to data");
    DEBUG("Data: ", data.c_str());
    uint16_t crc = crc16(data);
    data += "," + std::to_string(crc);
}

/**
 * @brief Checks if the CRC in the given data matches the calculated CRC.
 *
 * This function extracts the CRC value from the provided data string, calculates the CRC16 checksum
 * for the data (excluding the CRC part), and compares it with the extracted CRC. It returns true if
 * the CRCs match, otherwise false.
 *
 * @param data The data string containing the CRC value appended at the end.
 *
 * @return bool Returns true if the extracted CRC matches the calculated CRC, false otherwise.
 *
 * @note The CRC value is expected to be the last part of the data, separated by a comma.
 */
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

/**
 * @brief Saves all settings to a file with a CRC checksum.
 *
 * This function iterates through the provided settings, appends each to a temporary string,
 * adds a CRC checksum, and then writes the resulting data to a file called "settings.txt".
 * If the number of settings is insufficient or there is a file write failure, it returns an error code.
 *
 * @param settings A vector of strings containing the settings to be saved.
 *
 * @return int
 * - 0: Success.
 * - 1: Insufficient settings provided.
 * - 2: File write failure.
 *
 * @note The function expects the settings vector to contain at least `Settings::CRC - 1` settings.
 * The CRC is appended to the settings before saving.
 */
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

/**
 * @brief Reads all settings from a file and validates the CRC checksum.
 *
 * This function attempts to read settings from a file called "settings.txt", checks the CRC of the
 * retrieved settings, and parses them into the provided vector. Returns an error code on failure.
 *
 * @param settings A vector of strings to store the retrieved settings.
 *
 * @return int
 * - 0: Success.
 * - 1: File opening failure.
 * - 2: File reading failure.
 * - 3: CRC check failure.
 * - 4: Malformed settings data.
 *
 * @note The function reads `Settings::CRC` lines from the file and expects the last line to be the CRC.
 * The settings are extracted and stored in the provided vector.
 */
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

/**
 * @brief Saves a single setting by updating its value.
 *
 * This function reads the current settings, updates the specified setting identified by `settingID`
 * with the provided value, and then saves all settings back to the file including crc. Returns an error code on
 * failure.
 *
 * @param settingID The identifier of the setting to be updated.
 * @param value The new value to be assigned to the setting.
 *
 * @return int
 * - 0: Success.
 * - 1: Failed to read settings.
 * - 2: Failed to save settings.
 *
 * @note The function modifies a specific setting and saves all settings to the file.
 */
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

/**
 * @brief Reads a single setting's value.
 *
 * This function reads all settings from the file and retrieves the value of the specified setting
 * identified by `settingID`. The value is assigned to the provided reference parameter.
 *
 * @param settingID The identifier of the setting to be read.
 * @param value A reference to the string where the setting value will be stored.
 *
 * @return int
 * - 0: Success.
 * - 1: Failed to read settings.
 *
 * @note The function retrieves the setting value based on its identifier and stores it in the provided reference.
 */
int SDcard::read_setting(const Settings settingID, std::string &value) {
    std::vector<std::string> settings;
    if (read_all_settings(settings) != 0) {
        DEBUG("Failed to read settings");
        return 1; // Failed to read settings
    }

    value = settings[(int)settingID];
    return 0; // Success
}
