#include "sd-card.hpp"
#include "ScopedMutex.hpp"
#include "crc.hpp"
#include "debug.hpp"
#include "defines.hpp"
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "mbedtls/base64.h"
#include "scopedBuffer.hpp"
#include "sdmmc_cmd.h"
#include <map>
#include <string.h>
#include <string>

SDcardHandler::SDcardHandler(Sd_card_mount_settings settings) {
    for (int i = 0; i < 3; i++) {
        this->sd_card_status = this->mount_sd_card(settings);
        if (this->sd_card_status == ESP_OK) {
            DEBUG("SD card mounted at ", settings.mount_point.c_str());
            this->file_mutex = xSemaphoreCreateMutex();
            return;
        }
        DEBUG("SD card mount failed, attempt ", i + 1);
        this->unmount_sdcard();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    DEBUG("Failed to mount SD card after 3 retries, error: ", esp_err_to_name(sd_card_status));
    this->sd_card_status = ESP_FAIL;
    return;
}

SDcardHandler::~SDcardHandler() {
    if (this->sd_card_status == ESP_OK) {
        esp_vfs_fat_sdcard_unmount(this->mount_point.c_str(), this->esp_sdcard);
        DEBUG("SD card unmounted from ", this->mount_point.c_str());
    }

    if (this->file_mutex) {
        vSemaphoreDelete(this->file_mutex);
        this->file_mutex = nullptr;
    }
}

esp_err_t SDcardHandler::mount_sd_card(Sd_card_mount_settings settings) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {.format_if_mount_failed = false,
                                                     .max_files = settings.max_open_files,
                                                     .allocation_unit_size = 16 * 1024,
                                                     .disk_status_check_enable = false,
                                                     .use_one_fat = false};

    this->mount_point = settings.mount_point;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode((gpio_num_t)settings.CMD, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)settings.D0, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)settings.D1, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)settings.D2, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)settings.D3, GPIO_PULLUP_ONLY);

    return esp_vfs_fat_sdmmc_mount(this->mount_point.c_str(), &host, &slot_config, &mount_config, &this->esp_sdcard);
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

    ScopedBuffer<char> scoped_read_buffer(file_size);

    size_t read_size = fread(scoped_read_buffer.get_pointer(), 1, file_size, file);
    if (read_size != file_size) {
        DEBUG("Failed to read the complete file");
        fclose(file);
        return 3;
    }

    DEBUG("Read size: ", read_size);
    read_data_storage.assign(scoped_read_buffer.get_pointer(), read_size);
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

    ScopedBuffer<char> scoped_read_buffer(file_size);
    size_t read_size = fread(scoped_read_buffer.get_pointer(), 1, file_size, file);
    if (read_size != file_size) {
        DEBUG("Failed to read the complete file");
        fclose(file);
        return 4;
    }

    // Calculate required Base64 buffer size
    size_t base64_len = ((read_size + 2) / 3) * 4;
    ScopedBuffer<unsigned char> scoped_base64_buffer(base64_len + 1);

    size_t actual_base64_len;
    int ret = mbedtls_base64_encode(scoped_base64_buffer.get_pointer(), base64_len + 1, &actual_base64_len,
                                    (const unsigned char *)scoped_read_buffer.get_pointer(), read_size);

    if (ret != 0) {
        DEBUG("Base64 encoding failed");
        fclose(file);
        return 5;
    }

    scoped_base64_buffer.get_pointer()[actual_base64_len] = '\0';
    read_data_storage.assign((char *)scoped_base64_buffer.get_pointer());

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
    DEBUG("Checking CRC of data: ", data.c_str());
    std::string copy = data;
    size_t pos = copy.find_last_of(',');
    if (pos == std::string::npos) { return false; }
    std::string crc_str = copy.substr(pos + 1);
    uint16_t crc = std::stoi(crc_str);
    copy = copy.substr(0, pos);
    uint16_t crc_calc = crc16(copy);
    DEBUG("CRC: ", crc, " CRC calc: ", crc_calc);
    return (crc == crc_calc);
}

int SDcardHandler::save_all_settings(const std::unordered_map<Settings, std::string> settings) {
    DEBUG("Saving all settings");

    std::string temp_settings;
    for (int i = 0; i < (int)Settings::CRC; i++) {
        temp_settings += settings.at((Settings)i);
        temp_settings += "\n";
    }

    add_crc(temp_settings);

    if (write_file("settings.txt", temp_settings)) {
        DEBUG("Failed to write settings to file");
        return 1; // File write failure
    }

    return 0; // Success
}

int SDcardHandler::read_all_settings(std::unordered_map<Settings, std::string> &settings) {
    DEBUG("Reading all settings");
    std::string full_filename_str = this->mount_point + "/settings.txt";
    std::string temp_settings;
    DEBUG("Opening file: ", full_filename_str.c_str());

    FILE *fptr = fopen(full_filename_str.c_str(), "r");
    if (!fptr) {
        DEBUG("Failed to open file for reading");
        return 1; // File opening failure
    }

    std::string line;
    char read_buffer[LINE_READ_BUFFER_SIZE];

    while (fgets(read_buffer, sizeof(read_buffer), fptr)) {
        line = read_buffer;
        temp_settings += line;
        DEBUG("Read: ", line.c_str());
    }

    fclose(fptr);

    if (!check_crc(temp_settings)) {
        DEBUG("CRC check failed");
        return 3; // CRC check failure
    }

    DEBUG("CRC check passed");

    size_t pos = 0;
    for (int i = 0; i < static_cast<int>(Settings::CRC); i++) {
        pos = temp_settings.find("\n");

        if (pos == std::string::npos) {
            DEBUG("Malformed settings data");
            return 4;
        }

        if (i == static_cast<int>(Settings::WEB_CERTIFICATE)) {
            pos = temp_settings.find("-----END CERTIFICATE-----");
            if (pos == std::string::npos) {
                DEBUG("Malformed certificate data");
                return 5;
            }
            pos += 25;
        }

        settings[static_cast<Settings>(i)] = temp_settings.substr(0, pos);
        temp_settings.erase(0, pos + 1);
    }

    return 0; // Success
}

int SDcardHandler::save_setting(const Settings settingID, const std::string &value) {
    std::unordered_map<Settings, std::string> settings;
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
    std::unordered_map<Settings, std::string> settings;
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