#include "date_utils.hpp"
#include "ctime"

/**
 * @brief increments datetime by one hour
 * @param date the datetime_t object to increment
 * @note Modifies the datetime object.
 */
void datetime_increment_hour(datetime_t &date) {
    date.hour++;
    if (date.hour >= 24) {
        date.hour = 0;
        datetime_increment_day(date);
    }
}
/**
 * @brief increments datetime by one minute
 * @param date the datetime_t object to increment
 * @note Modifies the datetime object.
 */
void datetime_increment_minute(datetime_t &date) {
    date.min++;
    if (date.min >= 60) {
        date.min = 0;
        datetime_increment_hour(date);
    }
}

/**
 * @brief adds hour to a datetime object
 * @param date the datetime_t object to increment
 * @param hours Number of hours to add to the datetime_t object
 * @note Modifies the datetime object.
 */
void datetime_add_hours(datetime_t &date, uint hours) {
    uint days_to_add = (date.hour + hours) / 24;
    date.hour = (date.hour + hours) - (days_to_add * 24);
    for (uint i = 0; i < days_to_add; i++)
        datetime_increment_day(date);
}
/**
 * @brief increments datetime by one day
 * @param date the datetime_t object to increment
 * @note Modifies the datetime object.
 */
void datetime_increment_day(datetime_t &date) {
    date.day++;
    switch (date.month) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            if (date.day > 31) date.day = 1;
            break;
        case 4:
        case 6:
        case 9:
        case 11:
            if (date.day > 30) date.day = 1;
            break;
        case 2:
            if (is_leap_year(date.year)) {
                if (date.day > 29) date.day = 1;
            } else {
                if (date.day > 28) date.day = 1;
            }
            break;
        default:
            break;
    }
}
/**
 * @brief increments datetime by one month
 * @param date the datetime_t object to increment
 * @note Modifies the datetime object.
 */
void datetime_increment_month(datetime_t &date) {
    date.month++;
    if (date.month > 12) { date.month = 1; }
}
/**
 * @brief increments datetime by one year
 * @param date the datetime_t object to increment
 * @note Modifies the datetime object.
 */
void datetime_increment_year(datetime_t &date) { date.year++; }
/**
 * @brief Checks if a year is a leap year
 * @param year the year to check
 * @return true if year is a leap year
 * @return false if year is not a leap year
 * 
 */
bool is_leap_year(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }
/**
 * @brief Converts datetime to number of seconds since unix epoch
 * @param dt the datetime object to convert
 * @return Number of seconds since epoch
 * 
 */
long long datetime_to_seconds(const datetime_t &dt) {
    std::tm tm_struct = {};
    tm_struct.tm_year = dt.year - 1900; // Years since 1900
    tm_struct.tm_mon = dt.month - 1;    // Months since January (0-11)
    tm_struct.tm_mday = dt.day;
    tm_struct.tm_hour = dt.hour;
    tm_struct.tm_min = dt.min;
    tm_struct.tm_sec = dt.sec;
    return std::mktime(&tm_struct);
}
/**
 * @brief Calculates the difference in hours between two datetime objects
 * @param dt1 First datetime object to compare
 * @param dt2 Second datetime object to compare
 * @return Number of hours between the two datetime objects
 */
int calculate_hour_difference(const datetime_t &dt1, const datetime_t &dt2) {
    long long diff_seconds = calculate_sec_difference(dt1, dt2);
    long long diff_hours = diff_seconds / 3600;

    return static_cast<int>(diff_hours);
}
/**
 * @brief Calculates the difference in seconds between two datetime objects
 * @param dt1 First datetime object to compare
 * @param dt2 Second datetime object to compare
 * @return Number of seconds between the two datetime objects
 */
int calculate_sec_difference(const datetime_t &dt1, const datetime_t &dt2) {
    long long seconds1 = datetime_to_seconds(dt1);
    long long seconds2 = datetime_to_seconds(dt2);
    if (seconds1 == -1 || seconds2 == -1) {
        // Handle invalid datetime_t values (mktime failed)
        return 0; // Or some error code
    }
    return static_cast<int>(seconds2 - seconds1);
}
/**
 * @brief Converts datetime to unix timestamp
 * @param date Datetime object to convert
 * @return Unix timestamp
 */
int64_t datetime_to_epoch(datetime_t date) {
    return datetime_to_epoch(date.year, date.month, date.day, date.hour, date.min, date.sec);
}
