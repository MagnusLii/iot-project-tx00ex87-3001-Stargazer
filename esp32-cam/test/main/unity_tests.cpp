#include "camera.hpp"
#include "test_jsonParser.hpp"
#include "test_sd-card.hpp"
#include "unity.h"

// #define JSON_PARSER_TESTS
// #define SDCARD_TESTS
// #define ALL_TESTS

#ifdef ALL_TESTS
#define JSON_PARSER_TESTS
#define SDCARD_TESTS
#endif // ALL_TESTS

extern "C" {
void app_main(void);
void setUp(void);
void tearDown(void);
}

void setUp() {}

void tearDown() {}

void app_main(void) {

    UNITY_BEGIN();
#ifdef JSON_PARSER_TESTS
    run_all_json_tests();
#endif // JSON_PARSER_TESTS

#ifdef SDCARD_TESTS
    run_all_sd_card_tests();
#endif // SDCARD_TESTS

#ifdef CAMERA_TESTS
    run_all_camera_tests();
#endif // CAMERA_TESTS

    UNITY_END();
}
