#include "date_utils.hpp"
#include "ctime"

void datetime_increment_hour(datetime_t &date) {
    date.hour++;
    if (date.hour >= 24) {
        date.hour = 0;
        datetime_increment_day(date);
    }
}

void datetime_increment_minute(datetime_t &date) {
    date.min++;
    if (date.min >= 60) {
        date.min = 0;
        datetime_increment_hour(date);
    }
}


void datetime_add_hours(datetime_t &date, uint hours) {
    uint days_to_add = (date.hour + hours) / 24;
    date.hour = (date.hour + hours) - (days_to_add * 24);
    for (uint i = 0; i < days_to_add; i++)
        datetime_increment_day(date);
}

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

void datetime_increment_month(datetime_t &date) {
    date.month++;
    if (date.month > 12) { date.month = 1; }
}

void datetime_increment_year(datetime_t &date) { date.year++; }

bool is_leap_year(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

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

int calculate_hour_difference(const datetime_t &dt1, const datetime_t &dt2) {
    long long diff_seconds = calculate_sec_difference(dt1, dt2);
    long long diff_hours = diff_seconds / 3600;

    return static_cast<int>(diff_hours);
}

int calculate_sec_difference(const datetime_t &dt1, const datetime_t &dt2) {
    long long seconds1 = datetime_to_seconds(dt1);
    long long seconds2 = datetime_to_seconds(dt2);
    if (seconds1 == -1 || seconds2 == -1) {
        // Handle invalid datetime_t values (mktime failed)
        return 0; // Or some error code
    }
    return static_cast<int>(seconds2 - seconds1);
}

int64_t datetime_to_epoch(datetime_t date) {
    return datetime_to_epoch(date.year, date.month, date.day, date.hour, date.min, date.sec);
}

/*
eka:
tunti 5
päivä 31
kuukausi 12
toka:
tunti 2
päivä 1
kuukausi 1
*/
