#include "camera.hpp"
#include "test_jsonParser.hpp"
#include "test_sd-card.hpp"
#include "test_requestHandler.hpp"
#include "unity.h"

// #define JSON_PARSER_TESTS
// #define SDCARD_TESTS
#define REQUESTHANDLER_TESTS
// #define ALL_TESTS

#ifdef ALL_TESTS
#define JSON_PARSER_TESTS
#define SDCARD_TESTS
#define REQUESTHANDLER_TESTS
#endif // ALL_TESTS

extern "C" {
void app_main(void);
void setUp(void);
void tearDown(void);
}
void app_main(void) {

    UNITY_BEGIN();
#ifdef JSON_PARSER_TESTS
    run_all_json_tests();
#endif // JSON_PARSER_TESTS

#ifdef SDCARD_TESTS
    run_all_sd_card_tests();
#endif // SDCARD_TESTS

#ifdef REQUESTHANDLER_TESTS
    run_all_request_handler_tests();
#endif // REQUESTHANDLER_TESTS

    UNITY_END();
}
