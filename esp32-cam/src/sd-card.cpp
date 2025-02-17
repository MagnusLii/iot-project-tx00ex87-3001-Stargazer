#include "sd-card.hpp"
#include "crc.hpp"
#include "debug.hpp"
#include "defines.hpp"
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "mbedtls/base64.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <string>
#include <map>
#include "ScopedMutex.hpp"

/**
 * @brief Initializes the SDcardHandler object and attempts to mount the SD card.
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
SDcardHandler::SDcardHandler(std::string mount_point_arg, int max_open_files, int CMD, int D0, int D1, int D2, int D3) {
    for (int i = 0; i < 3; i++) {
        this->sd_card_status = this->mount_sd_card(mount_point_arg, max_open_files, CMD, D0, D1, D2, D3);
        if (this->sd_card_status == ESP_OK) {
            DEBUG("SD card mounted at ", mount_point.c_str());
            this->file_mutex = xSemaphoreCreateMutex();
            return;
        }
        DEBUG("SD card mount failed, attempt ", i + 1);
        this->unmount_sdcard();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    DEBUG("Failed to mount SD card after ", RETRIES, " retries, error: ", esp_err_to_name(sd_card_status));
    this->sd_card_status = ESP_FAIL;
    return;
}

SDcardHandler::~SDcardHandler() {
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
esp_err_t SDcardHandler::mount_sd_card(std::string mount_point_arg, int max_open_files, int CMD, int D0, int D1, int D2,
                                       int D3) {

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {.format_if_mount_failed = false,
                                                     .max_files = max_open_files,
                                                     .allocation_unit_size = 16 * 1024,
                                                     .disk_status_check_enable = false,
                                                     .use_one_fat = false};

    const char *mount_point = mount_point_arg.c_str();
    this->mount_point = mount_point_arg;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode((gpio_num_t)CMD, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)D0, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)D1, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)D2, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)D3, GPIO_PULLUP_ONLY);

    return esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &this->esp_sdcard);
}

/**
 * @brief Returns a pointer to the mount point string.
 *
 * @return std::string* A pointer to the mount point string.
 */
std::string *SDcardHandler::get_mount_point() { return &this->mount_point; }

/**
 * @brief Returns the mount point as a C-style string.
 *
 * @return const char* A constant pointer to the C-style string representing the mount point.
 */
const char *SDcardHandler::get_mount_point_c_str() { return this->mount_point.c_str(); }

/**
 * @brief Returns the current status of the SD card.
 *
 * @return esp_err_t The current SD card status (e.g., ESP_OK or error code).
 */
esp_err_t SDcardHandler::get_sd_card_status() { return this->sd_card_status; }

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
int SDcardHandler::write_file(const char *filename, std::string data) {
    ScopedMutex lock(file_mutex); // Automatically locks and unlocks the mutex

    std::string full_filename_str = this->mount_point + "/" + filename;

    DEBUG("Opening file ", full_filename_str.c_str());
    FILE *file = fopen(full_filename_str.c_str(), "w");

    if (!file) {
        DEBUG("Failed to open file for writing");
        perror("File open error");
        return 2; // File open failure
    }

    if (fwrite(data.c_str(), 1, data.length(), file) != data.length()) {
        DEBUG("File write failed");
        fclose(file);
        return 3; // File write failure
    }

    DEBUG("File write success");
    fclose(file);
    return 0; // Success
}

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
int SDcardHandler::write_file(const char *filename, const char *data) {
    return write_file(filename, std::string(data));
}

/**
 * @brief Writes binary data to a file on the SD card.
 *
 * This function prepends the mount point to the given filename, attempts to open the file for writing,
 * and writes the provided data to the file. Utilizes mutex for thread safety and returns an error code on failure.
 *
 * @param filename The name of the file to write to, including the file extension.
 * @param data Pointer to the binary data to be written.
 * @param len The length of the data in bytes.
 *
 * @return int Returns:
 *  - `0`: Success.
 *  - `1`: Mutex acquisition failure.
 *  - `2`: File open failure.
 *  - `3`: File write failure.
 *
 * @note The function locks a mutex to ensure that file operations are thread-safe.
 */
int SDcardHandler::write_file(const char *filename, const uint8_t *data, const size_t len) {
    ScopedMutex lock(file_mutex); // Automatically locks and unlocks the mutex

    std::string full_filename_str = this->mount_point + "/" + filename;

    DEBUG("Opening file ", full_filename_str.c_str());
    FILE *file = fopen(full_filename_str.c_str(), "w");

    if (!file) {
        DEBUG("Failed to open file for writing");
        perror("File open error");
        return 2; // File open failure
    }

    if (fwrite(data, 1, len, file) != len) {
        DEBUG("File write failed");
        fclose(file);
        return 3; // File write failure
    }

    DEBUG("File write success");
    fclose(file);
    return 0; // Success
}

int SDcardHandler::read_file(const char *filename, std::string &read_data_storage) {
    ScopedMutex lock(file_mutex); // Automatically locks and unlocks the mutex

    std::string full_filename_str = this->mount_point + "/" + filename;
    DEBUG("Opening file ", full_filename_str.c_str());
    FILE *file = fopen(full_filename_str.c_str(), "rb"); // "rb" for binary-safe reading
    if (!file) {
        DEBUG("Failed to open file for reading");
        perror("File open error");
        return 2;
    }
    DEBUG("File opened successfully");

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    DEBUG("File size: ", file_size);
    char *read_buffer = new char[file_size];
    size_t read_size = fread(read_buffer, 1, file_size, file);
    if (read_size != file_size) {
        DEBUG("Failed to read the complete file");
        fclose(file);
        delete[] read_buffer;
        return 3;
    }

    DEBUG("Read size: ", read_size);
    read_data_storage.assign(read_buffer, read_size);
    delete[] read_buffer;
    fclose(file);

    DEBUG("Exiting read_file");
    return 0;
}


int SDcardHandler::read_file_base64(const char *filename, std::string &read_data_storage) {
    ScopedMutex lock(file_mutex); // Automatically locks and unlocks the mutex

    std::string full_filename_str = this->mount_point + "/" + filename;
    FILE *file = fopen(full_filename_str.c_str(), "rb"); // "rb" for binary-safe reading
    if (!file) {
        DEBUG("Failed to open file for reading");
        perror("File open error");
        return 2;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    if (file_size == 0) {
        DEBUG("File is empty");
        fclose(file);
        return 3;
    }

    char *read_buffer = new char[file_size];
    size_t read_size = fread(read_buffer, 1, file_size, file);
    if (read_size != file_size) {
        DEBUG("Failed to read the complete file");
        fclose(file);
        delete[] read_buffer;
        return 4;
    }

    // Calculate required Base64 buffer size
    size_t base64_len = ((read_size + 2) / 3) * 4;
    unsigned char *base64_buffer = new unsigned char[base64_len + 1];

    size_t actual_base64_len;
    int ret = mbedtls_base64_encode(base64_buffer, base64_len + 1, &actual_base64_len,
                                    (const unsigned char *)read_buffer, read_size);
    delete[] read_buffer;

    if (ret != 0) {
        DEBUG("Base64 encoding failed");
        fclose(file);
        delete[] base64_buffer;
        return 5;
    }

    base64_buffer[actual_base64_len] = '\0';
    read_data_storage.assign((char *)base64_buffer);
    delete[] base64_buffer;

    fclose(file);
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
void SDcardHandler::add_crc(std::string &data) {
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
bool SDcardHandler::check_crc(const std::string &data) {
    std::string copy = data;
    size_t pos = copy.find_last_of(',');
    if (pos == std::string::npos) { return false; }
    std::string crc_str = copy.substr(pos + 1);
    uint16_t crc = std::stoi(crc_str);
    copy = copy.substr(0, pos);
    uint16_t crc_calc = crc16(copy);
    return (crc == crc_calc);
}

int SDcardHandler::save_all_settings(const std::map <Settings, std::string> settings) {
    DEBUG("Saving all settings");

    std::string temp_settings;
    for (int i = 0; i < (int)Settings::CRC; i++) {
        temp_settings += settings.at((Settings)i);
    }

    add_crc(temp_settings);

    if (write_file("settings.txt", temp_settings)) {
        DEBUG("Failed to write settings to file");
        return 1; // File write failure
    }

    return 0; // Success
}

int SDcardHandler::read_all_settings(std::map <Settings, std::string> &settings) {
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

    for (int i = 0; i <= (int)Settings::CRC; i++) {
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

    fclose(fptr);

    if (!check_crc(temp_settings)) {
        DEBUG("CRC check failed");
        return 3; // CRC check failure
    }

    DEBUG("CRC check passed");

    size_t pos = 0;
    for (int i = 0; i < (int)Settings::CRC; i++) {
        pos = temp_settings.find("\n");
        if (pos == std::string::npos) {
            DEBUG("Malformed settings data");
            return 4; // Malformed settings data
        }
        settings[(Settings)i] = temp_settings.substr(0, pos).c_str();
        temp_settings.erase(0, pos + 1);
    }

    return 0; // Success
}

int SDcardHandler::save_setting(const Settings settingID, const std::string &value) {
    std::map <Settings, std::string> settings;
    if (read_all_settings(settings) != 0) {
        DEBUG("Failed to read settings");
        return 1; // Failed to read settings
    }

    settings[settingID] = value;
    if (save_all_settings(settings) != 0) {
        DEBUG("Failed to save settings");
        return 2; // Failed to save settings
    }

    return 0; // Success
}

int SDcardHandler::read_setting(const Settings settingID, std::string &value) {
    std::map <Settings, std::string> settings;
    if (read_all_settings(settings) != 0) {
        DEBUG("Failed to read settings");
        return 1; // Failed to read settings
    }

    value = settings[settingID];
    return 0; // Success
}

uint32_t SDcardHandler::get_sdcard_free_space() {
    FATFS *fs;
    DWORD free_clusters, total_clusters;
    uint32_t sector_size = this->esp_sdcard->csd.sector_size;

    // Get the filesystem information
    if (f_getfree("0:", &free_clusters, &fs) == FR_OK) {
        total_clusters = fs->n_fatent - 2;
        uint32_t free_sectors = free_clusters * fs->csize;
        uint32_t total_sectors = total_clusters * fs->csize;

        DEBUG("Total space: ", (total_sectors * sector_size / 1024), " KB");
        DEBUG("Free space: ", (free_sectors * sector_size / 1024), " KB");
        return free_sectors * sector_size;
    } else {
        DEBUG("Failed to get filesystem information");
        return 0;
    }
}

esp_err_t SDcardHandler::unmount_sdcard() {
    if (this->esp_sdcard == nullptr) {
        DEBUG("SD card already unmounted or not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = esp_vfs_fat_sdcard_unmount("/sdcard", this->esp_sdcard);
    if (err == ESP_OK) {
        DEBUG("SD card unmounted successfully");
    } else {
        DEBUG("Failed to unmount SD card: ", esp_err_to_name(err));
    }
    return err;
}


esp_err_t SDcardHandler::clear_sdcard() {
    esp_err_t ret;

    // Unmount SD card
    this->unmount_sdcard();

    // Format SD card (this will clear everything)
    ret = esp_vfs_fat_sdcard_format(this->mount_point.c_str(), this->esp_sdcard);
    if (ret != ESP_OK) {
        DEBUG("Failed to format SD card. Error: ", esp_err_to_name(ret));
        return ret;
    }

    DEBUG("SD card formatted and cleared");
    return ESP_OK;
}