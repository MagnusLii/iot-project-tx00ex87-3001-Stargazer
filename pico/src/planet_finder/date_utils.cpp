#include "date_utils.hpp"

void datetime_increment_hour(datetime_t &date) {
    date.hour++;
    if (date.hour >= 24) {
        date.hour = 0;
        datetime_increment_day(date);
    }
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
    if (date.month > 12) {
        date.month = 1;
    }
}

void datetime_increment_year(datetime_t &date) {
    date.year++;
}

bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}