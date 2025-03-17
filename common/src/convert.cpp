/**
 * @file convert.cpp
 * @brief Implementation of various conversion functions.
 */

#include "convert.hpp"

/**
 * @brief Converts a string to an integer.
 *
 * @param str The input string.
 * @param result Reference to store the converted integer.
 * @param hex If true, the input string is expected to be in hexadecimal format.
 * @return bool True if the conversion is successful, False otherwise.
 */
bool str_to_int(std::string &str, int &result, bool hex) {
    std::stringstream ss(str);
    if (hex) { ss >> std::hex; }
    if (!(ss >> result)) { return false; }
    return true;
}

/**
 * @brief Converts a string to a vector of strings.
 *
 * @param str The input string.
 * @param delim The delimiter character.
 * @param vec Reference to store the resulting vector.
 * @return bool True if the conversion is successful, False otherwise.
 */
bool str_to_vec(const std::string &str, const char delim, std::vector<std::string> &vec) {
    std::stringstream ss(str);
    std::string token;
    bool result = false;

    while (std::getline(ss, token, delim)) {
        vec.push_back(token);
    }

    if (!vec.empty()) result = true;

    return result;
}

/**
 * @brief Converts an integer to a hexadecimal string.
 *
 * @param num The integer to convert.
 * @param result Reference to store the resulting hexadecimal string.
 * @param width The width of the resulting string (optional).
 * @param fill If true, the resulting string will be filled with leading zeros.
 * @param uppercase If true, the resulting string will be in uppercase.
 * @return bool True if the conversion is successful, False otherwise.
 */
bool num_to_hex_str(int num, std::string &result, int width, bool fill, bool uppercase) {
    std::stringstream ss;

    if (width > 0) { ss << std::setw(width); }

    if (fill) { ss << std::setfill('0'); }

    if (uppercase) { ss << std::uppercase; }

    if (!(ss << std::hex << num)) { return false; }
    result = ss.str();
    return true;
}

/**
 * @brief Converts a date and time to an epoch timestamp.
 *
 * @param year The year.
 * @param month The month.
 * @param day The day.
 * @param hour The hour.
 * @param min The minute.
 * @param sec The second.
 * @return int64_t The epoch timestamp.
 */
int64_t datetime_to_epoch(int year, int month, int day, int hour, int min, int sec) {
    struct tm time;
    time.tm_year = year - 1900;
    time.tm_mon = month - 1;
    time.tm_mday = day;
    time.tm_hour = hour;
    time.tm_min = min;
    time.tm_sec = sec;
    return mktime(&time);
}
