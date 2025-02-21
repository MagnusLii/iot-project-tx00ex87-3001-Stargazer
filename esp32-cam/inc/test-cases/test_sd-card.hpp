#ifndef TEST_SD_CARD_HPP
#define TEST_SD_CARD_HPP

#include <stdio.h>
#include "unity.h"
#include "sd-card.hpp"
#include <string>

TEST_CASE("initialize SD card handler", "[sdcard-handler]") {
    SDcardHandler handler("/sdcard");
    TEST_ASSERT_EQUAL(ESP_OK, handler.get_sd_card_status());
}

TEST_CASE("write to SD card", "[sdcard-handler]") {
    SDcardHandler handler("/sdcard");
    const char *filename = "test.txt";
    const char *data = "Hello, SD card!";
    TEST_ASSERT_EQUAL_INT(0, handler.write_file(filename, data));
}

TEST_CASE("read from SD card", "[sdcard-handler]") {
    SDcardHandler handler("/sdcard");
    const char *filename = "test.txt";
    std::string buffer;
    
    TEST_ASSERT_EQUAL_INT(0, handler.read_file(filename, buffer));
    TEST_ASSERT_EQUAL_STRING("Hello, SD card!", buffer.c_str());
}

TEST_CASE("calculate CRC", "[sdcard-handler]") {
    SDcardHandler handler("/sdcard");
    std::string data = "test data!";
    handler.add_crc(data);
    TEST_ASSERT_TRUE(handler.check_crc(data));
}

TEST_CASE("save and read settings", "[sdcard-handler]") {
    SDcardHandler handler("/sdcard");
    std::unordered_map<Settings, std::string> settings;
    settings[Settings::WIFI_SSID] = "test_ssid";
    settings[Settings::WIFI_PASSWORD] = "test_password";
    settings[Settings::WEB_DOMAIN] = "test_domain";
    settings[Settings::WEB_PORT] = "test_port";
    settings[Settings::WEB_TOKEN] = "test_token";

    TEST_ASSERT_EQUAL_INT(0, handler.save_all_settings(settings));

    std::unordered_map<Settings, std::string> read_settings;
    TEST_ASSERT_EQUAL_INT(0, handler.read_all_settings(read_settings));

    TEST_ASSERT_EQUAL_STRING("test_ssid", read_settings[Settings::WIFI_SSID].c_str());
    TEST_ASSERT_EQUAL_STRING("test_password", read_settings[Settings::WIFI_PASSWORD].c_str());
    TEST_ASSERT_EQUAL_STRING("test_domain", read_settings[Settings::WEB_DOMAIN].c_str());
    TEST_ASSERT_EQUAL_STRING("test_port", read_settings[Settings::WEB_PORT].c_str());
    TEST_ASSERT_EQUAL_STRING("test_token", read_settings[Settings::WEB_TOKEN].c_str());
}

#endif // TEST_SD_CARD_HPP