#include "unity.h"
#include "planet_finder.hpp"

void setUp(void) {

}

void tearDown(void) {

}

void test_datetime_to_j2000_day(void) {
    datetime_t date1(1990, 4, 19, 0, 0, 0, 0); // 19 april 1990, at 0:00 UT
    datetime_t date2(2024, 12, 15, 0, 20, 21, 0); // 15 Dec 2024 at 20:21 UTC
    float result1 = datetime_to_j2000_day(date1);
    float result2 = datetime_to_j2000_day(date2);
    printf("%.10f, %.10f\n", result1, result2);
    TEST_ASSERT_EQUAL_FLOAT(-3543, result1);
    TEST_ASSERT_EQUAL_FLOAT(9116.8479167, result2);
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

void test_local_sidereal_time(void) {
    float j1 = -3543.0; // 19 april 1990, at 0:00 UT
    float j2 = 9116.84765625; // 15 Dec 2024 at 20:21 UTC
    float result1 = local_sidereal_time(j1, 24.9384); // Helsinki
    float result2 = local_sidereal_time(j2, 24.9384);
    TEST_ASSERT_FLOAT_WITHIN(0.0001, 231.7677, result1);
    TEST_ASSERT_FLOAT_WITHIN(0.0001, 55.1635, result2);
    
}

int main() {
    stdio_init_all();
    std::cout << "hello" << std::endl;
    UNITY_BEGIN();
    RUN_TEST(test_datetime_to_j2000_day);
    RUN_TEST(test_normalize_degrees);
    RUN_TEST(test_normalize_radians);
    RUN_TEST(test_local_sidereal_time);
    UNITY_END();
    while (1) ;
}