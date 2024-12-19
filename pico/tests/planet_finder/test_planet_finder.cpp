#include "unity.h"
#include "planet_finder.hpp"


void setUp(void) {

}

void tearDown(void) {

}

void test_datetime_to_j2000_day(void) {
    datetime_t date1(1990, 4, 19, 0, 0, 0, 0); // 19 april 1990, at 0:00 UT
    datetime_t date2(2024, 12, 15, 0, 20, 21, 0); // 15 Dec 2024 at 20:21 UTC
    double result1 = datetime_to_j2000_day(date1);
    double result2 = datetime_to_j2000_day(date2);
    printf("%.10f, %.10f\n", result1, result2);
    TEST_ASSERT_DOUBLE_WITHIN(0.00001, -3543, result1);
    TEST_ASSERT_DOUBLE_WITHIN(0.00001, 9116.8479167, result2);
}

void test_normalize_degrees(void) {
    #define NUM_ANGLES 5
    double angles[NUM_ANGLES] = {0.0, 200.657, -200.234, 700.078, -700.1234};
    double expecteds[NUM_ANGLES] = {0.0, 200.657, 159.766, 340.078, 19.876600000000053};
    double result = 0;
    for (int i=0; i<NUM_ANGLES; i++) {
        result = normalize_degrees(angles[i]);
        TEST_ASSERT_DOUBLE_WITHIN(0.00001, expecteds[i], result);
    }
}

void test_normalize_radians(void) {
    #define NUM_ANGLES_RAD 5
    double angles[NUM_ANGLES_RAD] = {M_PI, 0.1, 3*M_PI-0.03, -M_PI, -6*M_PI+0.56};
    double expecteds[NUM_ANGLES_RAD] = {3.141592653589793, 0.1, 3.1115926535897938, 3.141592653589793, 0.5599999999999987};
    double result = 0;
    for (int i=0; i<NUM_ANGLES_RAD; i++) {
        result = normalize_radians(angles[i]);
        TEST_ASSERT_DOUBLE_WITHIN(0.00001, expecteds[i], result);
    }
}

void test_local_sidereal_time(void) {
    double j1 = -3543.0; // 19 april 1990, at 0:00 UT
    double j2 = 9116.847916666884; // 15 Dec 2024 at 20:21 UTC
    double result1 = local_sidereal_time(j1, 24.9384); // Helsinki
    double result2 = local_sidereal_time(j2, 24.9384);
    TEST_ASSERT_DOUBLE_WITHIN(0.00001, 231.7719322091, result1);
    TEST_ASSERT_DOUBLE_WITHIN(0.00001, 55.1677094926, result2);
    
}

int main() {
    stdio_init_all();
    std::cout << sizeof(float) << " hello " << sizeof(double) << std::endl;
    UNITY_BEGIN();
    RUN_TEST(test_datetime_to_j2000_day);
    RUN_TEST(test_normalize_degrees);
    RUN_TEST(test_normalize_radians);
    RUN_TEST(test_local_sidereal_time);
    UNITY_END();
    while (1) ;
}