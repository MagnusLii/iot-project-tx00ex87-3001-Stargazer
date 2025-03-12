#include "timesync-lib.hpp"
#include "debug.hpp"
#include "defines.hpp"
#include "esp_event.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <ctime>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

/**
 * @brief Initializes the SNTP (Simple Network Time Protocol) client.
 * 
 * This function configures and starts the SNTP service to synchronize the system time
 * with an external time server. It sets the operating mode to polling and specifies 
 * "time.google.com" as the time server.
 * 
 * @note This function should be called during system initialization to ensure accurate timekeeping.
 */
void initialize_sntp() {
    DEBUG("Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.google.com");
    esp_sntp_init();
}

/**
 * @brief Sets the system timezone based on daylight saving time.
 * 
 * Determines whether daylight saving time (DST) is active and sets the timezone 
 * accordingly. If DST is active, the timezone is set to Eastern European Summer Time (EEST), 
 * otherwise, it is set to Eastern European Time (EET).
 * 
 * @return timeSyncLibReturnCodes::SUCCESS if the timezone is set successfully.
 */
timeSyncLibReturnCodes set_tz() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    DEBUG("Setting timezone");
    if (timeinfo.tm_isdst) {
        set_timezone_to_eest();
    } else {
        set_timezone_to_eet();
    }
    DEBUG("Timezone set");
    return timeSyncLibReturnCodes::SUCCESS;
}

/**
 * @brief Synchronizes the system time with a given timestamp.
 * 
 * Sets the system time to the provided UNIX timestamp (in seconds). 
 * If the timestamp is invalid (negative), the function returns an error.
 * 
 * @param timestamp_in_sec The UNIX timestamp in seconds to set the system time to.
 * 
 * @return - timeSyncLibReturnCodes::SUCCESS if the time is set successfully.
 * @return - timeSyncLibReturnCodes::INVALID_ARGUMENT if the provided timestamp is invalid.
 * @return - timeSyncLibReturnCodes::SET_TIME_ERROR if setting the time fails.
 */
timeSyncLibReturnCodes sync_time(int64_t timestamp_in_sec) {
    if (timestamp_in_sec < 0) {
        DEBUG("Invalid timestamp: ", timestamp_in_sec);
        return timeSyncLibReturnCodes::INVALID_ARGUMENT;
    }

    struct timeval tv = {timestamp_in_sec, 0};

    if (settimeofday(&tv, NULL) == 0) {
        DEBUG("Time set to ", timestamp_in_sec);
        return timeSyncLibReturnCodes::SUCCESS;
    } else {
        DEBUG("Failed to set time to ", timestamp_in_sec);
        return timeSyncLibReturnCodes::SET_TIME_ERROR;
    }
}

/**
 * @brief Sets the system timezone.
 * 
 * Configures the system's timezone environment variable and updates the timezone settings.
 * 
 * @param tz The timezone string (e.g., "EET-2EEST,M3.5.0/3,M10.5.0/4").
 */
void set_timezone(const char *tz) {
    setenv("TZ", tz, 1);
    tzset();
}

/**
 * @brief Sets the system timezone after synchronizing time via SNTP.
 * 
 * This function initializes SNTP, waits for the system time to be set, 
 * and then applies the specified timezone setting.
 * 
 * @param timezone The timezone string to be set (e.g., "EET-2EEST,M3.5.0/3,M10.5.0/4").
 * @return timeSyncLibReturnCodes Returns SUCCESS on successful timezone update, 
 *         GENERAL_ERROR if time synchronization fails.
 */
timeSyncLibReturnCodes set_tz(const char *timezone) {
    initialize_sntp();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count) {
        DEBUG("Waiting for system time to be set... ");
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2020 - 1900)) {
        DEBUG("System time failed to set");
        return timeSyncLibReturnCodes::GENERAL_ERROR;

    } else {
        DEBUG("Time is synced, now: ", asctime(&timeinfo));
    }

    DEBUG("Setting timezone");
    set_timezone_general(timezone);
    DEBUG("Timezone set");
    print_local_time();
    return timeSyncLibReturnCodes::SUCCESS;
}

/**
 * @brief Sets the system timezone to Eastern European Time (EET).
 * 
 * This function attempts to set the timezone to "UTC-2" with a retry mechanism.
 * If successful, it applies the new timezone setting; otherwise, it returns an error.
 * 
 * @return timeSyncLibReturnCodes Returns SUCCESS if the timezone is set successfully, 
 *         SET_TIME_ERROR if all attempts fail.
 */
timeSyncLibReturnCodes set_timezone_to_eet() {
    int attempts = 0;
    while (attempts < RETRIES) {
        DEBUG("Setting timezone to EET, attempt ", attempts + 1);
        if (setenv("TZ", "UTC-2", 1) == 0) {
            tzset();
            DEBUG("Timezone set");
            return timeSyncLibReturnCodes::SUCCESS;
        }
        DEBUG("Failed to set timezone on attempt ", attempts + 1);
        attempts++;
    }
    DEBUG("Failed to set timezone after ", RETRIES, " attempts");
    return timeSyncLibReturnCodes::SET_TIME_ERROR;
}

/**
 * @brief Sets the system timezone to Eastern European Summer Time (EEST).
 * 
 * This function attempts to set the timezone to "UTC-3" with a retry mechanism.
 * If successful, it applies the new timezone setting; otherwise, it returns an error.
 * 
 * @return timeSyncLibReturnCodes Returns SUCCESS if the timezone is set successfully, 
 *         SET_TIME_ERROR if all attempts fail.
 */
timeSyncLibReturnCodes set_timezone_to_eest() {
    int attempts = 0;
    while (attempts < RETRIES) {
        DEBUG("Setting timezone to EEST, attempt ", attempts + 1);
        if (setenv("TZ", "UTC-3", 1) == 0) {
            tzset();
            DEBUG("Timezone set");
            return timeSyncLibReturnCodes::SUCCESS;
        }
        DEBUG("Failed to set timezone on attempt ", attempts + 1);
        attempts++;
    }
    DEBUG("Failed to set timezone after ", RETRIES, " attempts");
    return timeSyncLibReturnCodes::SET_TIME_ERROR;
}

/**
 * @brief Sets the system timezone to the specified value.
 * 
 * This function attempts to set the timezone using the provided timezone string.
 * It retries up to a defined number of times (RETRIES) in case of failure.
 * 
 * @param timezone The timezone string to be set (e.g., "UTC-3").
 * @return timeSyncLibReturnCodes Returns SUCCESS if the timezone is set successfully, 
 *         SET_TIME_ERROR if all attempts fail.
 */
timeSyncLibReturnCodes set_timezone_general(const char *timezone) {
    int attempts = 0;
    while (attempts < RETRIES) {
        DEBUG("Setting timezone to ", timezone, " attempt ", attempts + 1);
        if (setenv("TZ", timezone, 1) == 0) {
            tzset();
            DEBUG("Timezone set");
            return timeSyncLibReturnCodes::SUCCESS;
        }
        DEBUG("Failed to set timezone on attempt ", attempts + 1);
        attempts++;
    }
    DEBUG("Failed to set timezone after ", RETRIES, " attempts");
    return timeSyncLibReturnCodes::SET_TIME_ERROR;
}

/**
 * @brief Prints the current local time in the format DD/MM/YYYY HH:MM:SS.
 * 
 * This function retrieves the current system time, formats it, and outputs it using DEBUG.
 * If retrieving the local time fails, an error code is returned.
 * 
 * @return timeSyncLibReturnCodes Returns SUCCESS if the time is printed successfully,
 *         GET_TIME_ERROR if retrieving the local time fails.
 */
timeSyncLibReturnCodes print_local_time() {
    time_t now;
    time(&now);
    std::tm *timeinfo = std::localtime(&now);
    if (timeinfo == nullptr) { return timeSyncLibReturnCodes::GET_TIME_ERROR; }
    char day[3], month[4], year[5], hour[3], minute[3], second[3];

    strftime(day, sizeof(day), "%d", timeinfo);
    strftime(month, sizeof(month), "%m", timeinfo);
    strftime(year, sizeof(year), "%Y", timeinfo);
    strftime(hour, sizeof(hour), "%H", timeinfo);
    strftime(minute, sizeof(minute), "%M", timeinfo);
    strftime(second, sizeof(second), "%S", timeinfo);

    DEBUG(day, "/", month, "/", year, " ", hour, ":", minute, ":", second);
    return timeSyncLibReturnCodes::SUCCESS;
}

/**
 * @brief Retrieves the current local time and formats it as a string.
 * 
 * The function formats the current system time into the format "DD-MM-YYYY--HH-MM-SS" and stores it in the provided buffer.
 * If the buffer size is insufficient or if time retrieval fails, an error code is returned.
 * 
 * @param buffer A pointer to a character array where the formatted time string will be stored.
 * @param buffer_size The size of the provided buffer.
 * 
 * @return - timeSyncLibReturnCodes Returns SUCCESS if the time string is generated successfully,
 * @return - INCORRECT_BUFFER_SIZE if the buffer is too small to hold the time string,
 * @return - GET_TIME_ERROR if the local time retrieval fails.
 */
timeSyncLibReturnCodes get_localtime_string(char *buffer, int buffer_size) {
    if (buffer_size < 19) { return timeSyncLibReturnCodes::INCORRECT_BUFFER_SIZE; }

    time_t now;
    time(&now);
    std::tm *timeinfo = std::localtime(&now);
    if (timeinfo == nullptr) { return timeSyncLibReturnCodes::GET_TIME_ERROR; }
    char day[3], month[4], year[5], hour[3], minute[3], second[3];

    strftime(day, sizeof(day), "%d", timeinfo);
    strftime(month, sizeof(month), "%m", timeinfo);
    strftime(year, sizeof(year), "%Y", timeinfo);
    strftime(hour, sizeof(hour), "%H", timeinfo);
    strftime(minute, sizeof(minute), "%M", timeinfo);
    strftime(second, sizeof(second), "%S", timeinfo);

    sprintf(buffer, "%s-%s-%s--%s-%s-%s", day, month, year, hour, minute, second);
    return timeSyncLibReturnCodes::SUCCESS;
}

/**
 * @brief Retrieves the current local time and stores it in a `struct tm`.
 * 
 * This function gets the current system time and stores it in the provided `struct tm` object.
 * If the time retrieval fails, an error code is returned.
 * 
 * @param timeinfo A pointer to a `struct tm` where the current local time will be stored.
 * 
 * @return timeSyncLibReturnCodes Returns SUCCESS if the time is retrieved and stored successfully,
 *         GET_TIME_ERROR if there is a failure in retrieving the local time.
 */
timeSyncLibReturnCodes get_localtime_struct(struct tm *timeinfo) {
    time_t now;
    time(&now);
    *timeinfo = *std::localtime(&now);
    if (timeinfo == nullptr) { return timeSyncLibReturnCodes::GET_TIME_ERROR; }
    return timeSyncLibReturnCodes::SUCCESS;
}

/**
 * @brief Retrieves the current UTC time and stores it in a `struct tm`.
 * 
 * This function gets the current system time in UTC and stores it in the provided `struct tm` object.
 * If the time retrieval fails, an error code is returned.
 * 
 * @param timeinfo A pointer to a `struct tm` where the current UTC time will be stored.
 * 
 * @return timeSyncLibReturnCodes Returns SUCCESS if the UTC time is retrieved and stored successfully,
 *         GET_TIME_ERROR if there is a failure in retrieving the UTC time.
 */
timeSyncLibReturnCodes get_utc_struct(struct tm *timeinfo) {
    time_t now;
    time(&now);
    *timeinfo = *std::gmtime(&now);
    if (timeinfo == nullptr) { return timeSyncLibReturnCodes::GET_TIME_ERROR; }
    return timeSyncLibReturnCodes::SUCCESS;
}

/**
 * @brief Retrieves the current date and time as a timestamp.
 * 
 * This function returns the current system time as a timestamp in seconds since the Unix epoch (January 1, 1970).
 * 
 * @return int Returns the current date and time as a timestamp.
 */
int get_datetime() { return time(NULL); }