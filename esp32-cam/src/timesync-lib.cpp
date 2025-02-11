#include <stdio.h>
#include <time.h>
#include <ctime>
#include <sys/time.h>
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "debug.hpp"
#include "timesync-lib.hpp"
#include "defines.hpp"


void initialize_sntp() {
    DEBUG("Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.google.com");
    esp_sntp_init();
}

// Set's timezone to Finnish winter time using external SNTP server.
timeSyncLibReturnCodes set_tz() {
    initialize_sntp();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
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
        return timeSyncLibReturnCodes::SET_TIME_ERROR;

    } else {
        DEBUG("Time is synced, now: ", asctime(&timeinfo));
    }

    DEBUG("Setting timezone");
    if (timeinfo.tm_isdst) {
        set_timezone_to_eest();
    } else {
        set_timezone_to_eet();
    }
    DEBUG("Timezone set");
    print_local_time();
    return timeSyncLibReturnCodes::SUCCESS;
}

timeSyncLibReturnCodes sync_time(u64_t timestamp_in_sec){
    struct timeval tv = {timestamp_in_sec, 0};

    if(settimeofday(&tv, NULL) == 0){
        DEBUG("Time set to ", timestamp_in_sec);
        return timeSyncLibReturnCodes::SUCCESS;
    } else {
        DEBUG("Failed to set time to ", timestamp_in_sec);
        return timeSyncLibReturnCodes::SET_TIME_ERROR;
    }
}

void set_timezone(const char *tz) {
    setenv("TZ", tz, 1);
    tzset();
}

timeSyncLibReturnCodes set_tz(const char* timezone) {
    initialize_sntp();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
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

// FI daylight saving / winter time.
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

// FI summer time.
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

timeSyncLibReturnCodes set_timezone_general(const char* timezone) {
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

timeSyncLibReturnCodes print_local_time() {
    time_t now;
    time(&now);
    std::tm* timeinfo = std::localtime(&now);
    if (timeinfo == nullptr) {return timeSyncLibReturnCodes::GET_TIME_ERROR;}
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

timeSyncLibReturnCodes get_localtime_string(char* buffer, int buffer_size) {
    if (buffer_size < 19){
        return timeSyncLibReturnCodes::INCORRECT_BUFFER_SIZE;
    }

    time_t now;
    time(&now);
    std::tm* timeinfo = std::localtime(&now);
    if (timeinfo == nullptr) {return timeSyncLibReturnCodes::GET_TIME_ERROR;}
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

timeSyncLibReturnCodes get_localtime_struct(struct tm* timeinfo) {
    time_t now;
    time(&now);
    *timeinfo = *std::localtime(&now);
    if (timeinfo == nullptr) {return timeSyncLibReturnCodes::GET_TIME_ERROR;}
    return timeSyncLibReturnCodes::SUCCESS;
}

timeSyncLibReturnCodes get_utc_struct(struct tm* timeinfo) {
    time_t now;
    time(&now);
    *timeinfo = *std::gmtime(&now);
    if (timeinfo == nullptr) {return timeSyncLibReturnCodes::GET_TIME_ERROR;}
    return timeSyncLibReturnCodes::SUCCESS;
}

int get_datetime(){
    return time(NULL);
}