#include "convert.hpp"

bool str_to_int(std::string &str, int &result, bool hex) {
    std::stringstream ss(str);
    if (hex) { ss >> std::hex; }
    if (!(ss >> result)) { return false; }
    return true;
}

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

bool num_to_hex_str(int num, std::string &result, int width, bool fill, bool uppercase) {
    std::stringstream ss;

    if (width > 0) { ss << std::setw(width); }

    if (fill) { ss << std::setfill('0'); }

    if (uppercase) { ss << std::uppercase; }

    if (!(ss << std::hex << num)) { return false; }
    result = ss.str();
    return true;
}

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
