#include "test_jsonParser.hpp"

void test_parse_valid_json() {
    JsonParser parser;
    std::unordered_map<std::string, std::string> result;
    std::string json = "{\"key\":\"value\"}";
    int ret = parser.parse(json, &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("value", result["key"].c_str());
}

void test_parse_empty_json() {
    JsonParser parser;
    std::unordered_map<std::string, std::string> result;
    std::string json = "{}";
    int ret = parser.parse(json, &result);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

void test_parse_missing_colon() {
    JsonParser parser;
    std::unordered_map<std::string, std::string> result;
    std::string json = "{\"key\" \"value\"}";
    int ret = parser.parse(json, &result);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

void test_parse_null_pointer() {
    JsonParser parser;
    int ret = parser.parse("{\"key\":\"value\"}", NULL);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

void test_parse_empty_string() {
    JsonParser parser;
    std::unordered_map<std::string, std::string> result;
    std::string json = "";
    int ret = parser.parse(json, &result);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

void run_all_json_tests() {
    RUN_TEST(test_parse_valid_json);
    RUN_TEST(test_parse_empty_json);
    RUN_TEST(test_parse_missing_colon);
    RUN_TEST(test_parse_null_pointer);
    RUN_TEST(test_parse_empty_string);
}
