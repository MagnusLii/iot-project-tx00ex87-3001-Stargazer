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

void test_normalize_degrees(void) {
    #define NUM_ANGLES 5
    float angles[NUM_ANGLES] = {0.0, 200.657, -200.234, 700.078, -700.1234};
    float expecteds[NUM_ANGLES] = {0.0, 200.657, 159.766, 340.078, 19.876600000000053};
    float result = 0;
    for (int i=0; i<NUM_ANGLES; i++) {
        result = normalize_degrees(angles[i]);
        TEST_ASSERT_FLOAT_WITHIN(0.0001, expecteds[i], result);
    }
}

void test_normalize_radians(void) {
    #define NUM_ANGLES_RAD 5
    float angles[NUM_ANGLES_RAD] = {M_PI, 0.1, 3*M_PI-0.03, -M_PI, -6*M_PI+0.56};
    float expecteds[NUM_ANGLES_RAD] = {3.141592653589793, 0.1, 3.1115926535897938, 3.141592653589793, 0.5599999999999987};
    float result = 0;
    for (int i=0; i<NUM_ANGLES_RAD; i++) {
        result = normalize_radians(angles[i]);
        TEST_ASSERT_FLOAT_WITHIN(0.0001, expecteds[i], result);
    }
}

int main() {
    stdio_init_all();
    std::cout << "hello" << std::endl;
    UNITY_BEGIN();
    RUN_TEST(test_datetime_to_julian_day);
    RUN_TEST(test_normalize_degrees);
    UNITY_END();
    while (1) ;
}