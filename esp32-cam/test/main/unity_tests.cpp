#include "unity.h"
#include <stdio.h>
#include <string.h>

// #define JSON_PARSER_TESTS
#define SDCARD_TESTS
// #define FAILING_TESTS
// #define NON_FAILING_TESTS
// #define ALL_TESTS

#ifdef JSON_PARSER_TESTS
#include "test_jsonParser.hpp"
#endif // JSON_PARSER_TESTS

#ifdef SDCARD_TESTS
#include "test_sd-card.hpp"
#endif // SDCARD_TESTS

extern "C" {
void app_main(void);
}
void app_main(void) {

    UNITY_BEGIN();
#ifdef JSON_PARSER_TESTS
    unity_run_tests_by_tag("[json-parser]", false);
#endif // JSON_PARSER_TESTS

#ifdef SDCARD_TESTS
    unity_run_tests_by_tag("[sdcard-handler]", false);
#endif // SDCARD_TESTS

#ifdef FAILING_TESTS
    unity_run_tests_by_tag("[fail]", false);
#endif // FAILING_TESTS

#ifdef NON_FAILING_TESTS
    unity_run_tests_by_tag("[fail]", true);
#endif // NON_FAILING_TESTS

#ifdef ALL_TESTS
    unity_run_all_tests();
#endif // ALL_TESTS
    UNITY_END();
}
