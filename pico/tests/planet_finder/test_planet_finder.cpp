#include "unity.h"
#include "planet_finder.hpp"

#define DELTA 0.00001

void setUp(void) {

}

void tearDown(void) {

}

void test_datetime_to_j2000_day(void) {
    datetime_t date1(1990, 4, 19, 0, 0, 0, 0); // 19 april 1990, at 0:00 UT
    datetime_t date2(2024, 12, 15, 0, 20, 21, 0); // 15 Dec 2024 at 20:21 UTC
    double result1 = datetime_to_j2000_day(date1);
    double result2 = datetime_to_j2000_day(date2);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, -3543, result1);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 9116.8479167, result2);
}

void test_normalize_degrees(void) {
    #define NUM_ANGLES 5
    double angles[NUM_ANGLES] = {0.0, 200.657, -200.234, 700.078, -700.1234};
    double expecteds[NUM_ANGLES] = {0.0, 200.657, 159.766, 340.078, 19.876600000000053};
    double result = 0;
    for (int i=0; i<NUM_ANGLES; i++) {
        result = normalize_degrees(angles[i]);
        TEST_ASSERT_DOUBLE_WITHIN(DELTA, expecteds[i], result);
    }
}

void test_normalize_radians(void) {
    #define NUM_ANGLES_RAD 5
    double angles[NUM_ANGLES_RAD] = {M_PI, 0.1, 3*M_PI-0.03, -M_PI, -6*M_PI+0.56};
    double expecteds[NUM_ANGLES_RAD] = {3.141592653589793, 0.1, 3.1115926535897938, 3.141592653589793, 0.5599999999999987};
    double result = 0;
    for (int i=0; i<NUM_ANGLES_RAD; i++) {
        result = normalize_radians(angles[i]);
        TEST_ASSERT_DOUBLE_WITHIN(DELTA, expecteds[i], result);
    }
}

void test_local_sidereal_time(void) {
    double j1 = -3543.0; // 19 april 1990, at 0:00 UT
    double j2 = 9116.847916666884; // 15 Dec 2024 at 20:21 UTC
    double result1 = local_sidereal_time(j1, 24.9384); // Helsinki
    double result2 = local_sidereal_time(j2, 24.9384);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 231.7719322091, result1);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 55.1677094926, result2);
}

void test_obliquity_of_eplictic(void) {
    double j = 9116.847916666884;
    double result = obliquity_of_eplectic(j);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 23.43605166708729, result);
}

void test_coordinates(void) {
    rect_coordinates rcs[4] = {{1, 1, 1}, {0, -1, -1}, {-6.4, 0, 45.3}, {0, 0, 0}};
    spherical_coordinates scs[4] = {{0, 0, 0}, {-2, -3, 1}, {1.4, 4.76, 1}, {4.323546, -2.43654, -2.3245}};

    // convert from one system to other and back and see if the original and the manipulated one match
    for (int i=0; i<4, i++;) {
        spherical_coordinates sc = to_spherical_coordinates(rcs[i]);
        rect_coordinates rc = to_rectangular_coordinates(sc);
        TEST_ASSERT_DOUBLE_WITHIN(DELTA, rcs[i].x, rc.x);
        TEST_ASSERT_DOUBLE_WITHIN(DELTA, rcs[i].y, rc.y);
        TEST_ASSERT_DOUBLE_WITHIN(DELTA, rcs[i].z, rc.z);

        rect_coordinates rc2 = to_rectangular_coordinates(scs[i]);
        spherical_coordinates sc2 = to_spherical_coordinates(rc2);
        TEST_ASSERT_DOUBLE_WITHIN(DELTA, scs[i].RA, sc2.RA);
        TEST_ASSERT_DOUBLE_WITHIN(DELTA, scs[i].DECL, sc2.DECL);
        TEST_ASSERT_DOUBLE_WITHIN(DELTA, scs[i].distance, sc2.distance);
    }
}

void test_orbital_elements(void) {
    datetime_t date1(1990, 4, 19, 0, 0, 0, 0); // 19 april 1990, at 0:00 UT
    double j = datetime_to_j2000_day(date1);
    orbital_elements sun(j, SUN);
    orbital_elements moon(j, MOON);
    orbital_elements mercury(j, MERCURY);
    orbital_elements venus(j, VENUS);
    orbital_elements mars(j, MARS);
    orbital_elements jupiter(j, JUPITER);
    orbital_elements saturn(j, SATURN);
    orbital_elements uranus(j, URANUS);
    orbital_elements neptune(j, NEPTUNE);

    // sun
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.0, sun.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.0, sun.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 4.93532861, sun.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.0, sun.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.016713, sun.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.816282, sun.M);
    // moon
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 5.45830954, moon.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.08980417, moon.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.67107247, moon.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 60.2666, moon.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.054900, moon.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 4.644240, moon.M);
    // mercury
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.84153318, mercury.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.1222515, mercury.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.50768486, mercury.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.387098, mercury.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.205633, mercury.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.2132708, mercury.M);
    // venus
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.33679130, venus.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.0592452, venus.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.957173468, venus.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.723330, venus.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.006778, venus.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 2.29786209, venus.M);
    // mars
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.86363429, mars.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.03228510, mars.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 4.99858458, mars.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.523688, mars.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.093396, mars.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 5.61989910, mars.M);
    // jupiter
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.75154436, jupiter.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.022752112, jupiter.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 4.77905008, jupiter.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 5.20256, jupiter.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.048482, jupiter.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.492671898, jupiter.M);
    // saturn
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.982322275, saturn.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.04344124, saturn.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 5.921699693, saturn.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 9.55475, saturn.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.055580, saturn.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 3.4640265249, saturn.M);
    // uranus
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.29068843, uranus.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.013494885, uranus.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.685166007, uranus.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 19.18176, uranus.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.047292, uranus.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 1.763585395, uranus.M);
    // neptune
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 2.29813960, neptune.N);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.030908035, neptune.i);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 4.7624362966, neptune.w);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 30.05814, neptune.a);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 0.008598, neptune.e);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 4.171446868, neptune.M);
}

void test_perturbations(void) {
    datetime_t date1(1990, 4, 19, 0, 0, 0, 0); // 19 april 1990, at 0:00 UT
    double j = datetime_to_j2000_day(date1);
    orbital_elements moon(j, MOON);
    orbital_elements sun(j, SUN);
    orbital_elements jupiter(j, JUPITER);
    orbital_elements saturn(j, SATURN);
    orbital_elements uranus(j, URANUS);
    ecliptic_coordinates result_moon = perturbation_moon(moon, sun);
    ecliptic_coordinates result_jupiter = perturbation_jupiter(jupiter.M, saturn.M);
    ecliptic_coordinates result_saturn = perturbation_saturn(jupiter.M, saturn.M);
    ecliptic_coordinates result_uranus = perturbation_uranus(uranus.M, jupiter.M, saturn.M);
    // moon
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, -0.024664992, result_moon.lon);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, -0.0033492868, result_moon.lat);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, +0.0066, result_moon.distance); // the example used didn't give more accurate result, so we need to lower the delta.
    // jupiter
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, -0.000209439510, result_jupiter.lon);
    // saturn
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, -0.00121998514, result_saturn.lon);
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, 9.250245035569947e-05, result_saturn.lat);
    // uranus
    TEST_ASSERT_DOUBLE_WITHIN(DELTA, -0.000570722665, result_uranus.lon);
}


int main() {
    stdio_init_all();
    #ifdef ENABLE_DEBUG
    std::cout << sizeof(float) << " hello " << sizeof(double) << std::endl;
    #endif
    UNITY_BEGIN();
    RUN_TEST(test_datetime_to_j2000_day);
    RUN_TEST(test_normalize_degrees);
    RUN_TEST(test_normalize_radians);
    RUN_TEST(test_local_sidereal_time);
    RUN_TEST(test_obliquity_of_eplictic);
    RUN_TEST(test_coordinates);
    RUN_TEST(test_orbital_elements);
    RUN_TEST(test_perturbations);
    UNITY_END();
    while (1) ;
}