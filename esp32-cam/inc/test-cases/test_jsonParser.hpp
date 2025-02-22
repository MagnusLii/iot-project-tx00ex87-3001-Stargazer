#ifndef TEST_JSONPARSER_HPP
#define TEST_JSONPARSER_HPP

#include "jsonParser.hpp"
#include "unity.h"
#include <stdio.h>

void test_parse_valid_json();
void test_parse_empty_json();
void test_parse_missing_colon();
void test_parse_null_pointer();
void test_parse_empty_string();

void run_all_json_tests();

#endif // TEST_JSONPARSER_HPP