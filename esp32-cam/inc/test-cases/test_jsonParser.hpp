#ifndef TEST_JSONPARSER_HPP
#define TEST_JSONPARSER_HPP

#include <stdio.h>
#include "unity.h"
#include "jsonParser.hpp"

TEST_CASE("parse valid json", "[json-parser]") {
    JsonParser parser;
    std::map<std::string, std::string> result;
    std::string json = "{\"key\":\"value\"}";
    int ret = parser.parse(json, &result);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("value", result["key"].c_str());
}

TEST_CASE("parse empty json", "[json-parser]") {
    JsonParser parser;
    std::map<std::string, std::string> result;
    std::string json = "{}";
    int ret = parser.parse(json, &result);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

TEST_CASE("parse missing colon", "[json-parser]") {
    JsonParser parser;
    std::map<std::string, std::string> result;
    std::string json = "{\"key\" \"value\"}";
    int ret = parser.parse(json, &result);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

TEST_CASE("parse null pointer", "[json-parser]") {
    JsonParser parser;
    int ret = parser.parse("{\"key\":\"value\"}", NULL);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}

TEST_CASE("parse empty string", "[json-parser]") {
    JsonParser parser;
    std::map<std::string, std::string> result;
    std::string json = "";
    int ret = parser.parse(json, &result);
    TEST_ASSERT_NOT_EQUAL(0, ret);
}


#endif // TEST_JSONPARSER_HPP