#ifndef TEST_SD_CARD_HPP
#define TEST_SD_CARD_HPP

#include "sd-card.hpp"
#include "unity.h"
#include <stdio.h>
#include <string>
#include <unordered_map>

void test_initialize_sd_card_handler();
void test_write_to_sd_card();
void test_read_from_sd_card();
void test_calculate_crc();
void test_save_and_read_settings();

void run_all_sd_card_tests();

#endif // TEST_SD_CARD_HPP