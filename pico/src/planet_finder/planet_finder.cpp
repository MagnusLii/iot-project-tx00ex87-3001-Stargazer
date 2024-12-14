#include "planet_finder.hpp"
/*
PRIVATE
*/

float normalize_degrees(float degrees) {
    degrees = fmod(degrees, 360.0);
    if (degrees < 0) {
        degrees += 360.0;
    }
    return degrees;
}

float normalize_radians(float radians) {
    radians = fmod(radians, 2 * M_PI);
    if (radians < 0) {
        radians += 2 * M_PI;
    }
    return radians;
}


float datetime_to_julian_day(datetime_t &date) {
    int year = date.year;
    int month = date.month;
    float hour = (date.hour + (date.min / 60.0)) / 24.0;
    if (month <= 2) {
        month += 12;
        year--;
    }

    int year_calculation = (int)(365.25 * year);
    int month_calculation = (int)(30.6001 * (month + 1));
    return year_calculation + month_calculation - 15 + 1720996.5 + date.day + hour;
}