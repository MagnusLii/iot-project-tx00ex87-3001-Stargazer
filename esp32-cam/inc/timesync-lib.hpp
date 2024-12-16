#ifndef ESPTIMESET_HPP
#define ESPTIMESET_HPP
#include <string>

enum class timeSyncLibReturnCodes {
    SUCCESS,
    GENERAL_ERROR,
    GET_TIME_ERROR,
    SET_TIME_ERROR,
    INCORRECT_BUFFER_SIZE
};

void initialize_sntp();
timeSyncLibReturnCodes set_tz();
timeSyncLibReturnCodes set_timezone_to_eet();
timeSyncLibReturnCodes set_timezone_to_eest();
timeSyncLibReturnCodes set_timezone_general(const char* timezone);
timeSyncLibReturnCodes print_local_time();
timeSyncLibReturnCodes get_localtime_string(char* buffer, int buffer_size);
timeSyncLibReturnCodes get_localtime_struct(struct tm* timeinfo);
timeSyncLibReturnCodes get_utc_struct(struct tm* timeinfo);

#endif