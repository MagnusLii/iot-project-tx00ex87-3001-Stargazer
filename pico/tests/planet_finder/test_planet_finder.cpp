#include "unity.h"
#include "planet_finder.hpp"

void setUp(void) {

}

void tearDown(void) {

}

// typedef struct {
//     int16_t year;    ///< 0..4095
//     int8_t month;    ///< 1..12, 1 is January
//     int8_t day;      ///< 1..28,29,30,31 depending on month
//     int8_t dotw;     ///< 0..6, 0 is Sunday
//     int8_t hour;     ///< 0..23
//     int8_t min;      ///< 0..59
//     int8_t sec;      ///< 0..59
// } datetime_t;
void test_datetime_to_julian_day(void) {
    datetime_t date1(1991, 5, 19, 0, 13, 0, 0);
    float result1 = datetime_to_julian_day(date1);
    TEST_ASSERT_FLOAT_WITHIN(0.0001, 2448396.04167, result1);
}

int main() {
    stdio_init_all();
    std::cout << "hello" << std::endl;
    UNITY_BEGIN();
    RUN_TEST(test_datetime_to_julian_day);
    UNITY_END();
    while (1) ;
}