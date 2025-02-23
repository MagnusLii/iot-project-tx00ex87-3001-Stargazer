#include "test_sd-card.hpp"

void test_initialize_sd_card_handler() {
    Sd_card_mount_settings mount_settings;
    SDcardHandler handler(mount_settings);
    TEST_ASSERT_EQUAL(ESP_OK, handler.get_sd_card_status());
}

void test_write_to_sd_card() {
    Sd_card_mount_settings mount_settings;
    SDcardHandler handler(mount_settings);
    const char *filename = "test.txt";
    const char *data = "Hello, SD card!";
    TEST_ASSERT_EQUAL_INT(0, handler.write_file(filename, data));
}

void test_read_from_sd_card() {
    Sd_card_mount_settings mount_settings;
    SDcardHandler handler(mount_settings);
    const char *filename = "test.txt";
    std::string buffer;
    
    TEST_ASSERT_EQUAL_INT(0, handler.read_file(filename, buffer));
    TEST_ASSERT_EQUAL_STRING("Hello, SD card!", buffer.c_str());
}

void test_calculate_crc() {
    Sd_card_mount_settings mount_settings;
    SDcardHandler handler(mount_settings);
    std::string data = "test data!";
    handler.add_crc(data);
    TEST_ASSERT_TRUE(handler.check_crc(data));
}

void test_save_and_read_settings() {
    Sd_card_mount_settings mount_settings;
    SDcardHandler handler(mount_settings);
    std::unordered_map<Settings, std::string> settings;
    settings[Settings::WIFI_SSID] = "test_ssid";
    settings[Settings::WIFI_PASSWORD] = "test_password";
    settings[Settings::WEB_DOMAIN] = "test_domain";
    settings[Settings::WEB_PORT] = "test_port";
    settings[Settings::WEB_TOKEN] = "test_token";
    settings[Settings::WEB_CERTIFICATE] = "-----BEGIN CERTIFICATE-----\ntest_certificate\n-----END CERTIFICATE-----\n";

    TEST_ASSERT_EQUAL_INT(0, handler.save_all_settings(settings));

    std::unordered_map<Settings, std::string> read_settings;
    TEST_ASSERT_EQUAL_INT(0, handler.read_all_settings(read_settings));

    TEST_ASSERT_EQUAL_STRING("test_ssid", read_settings[Settings::WIFI_SSID].c_str());
    TEST_ASSERT_EQUAL_STRING("test_password", read_settings[Settings::WIFI_PASSWORD].c_str());
    TEST_ASSERT_EQUAL_STRING("test_domain", read_settings[Settings::WEB_DOMAIN].c_str());
    TEST_ASSERT_EQUAL_STRING("test_port", read_settings[Settings::WEB_PORT].c_str());
    TEST_ASSERT_EQUAL_STRING("test_token", read_settings[Settings::WEB_TOKEN].c_str());
    TEST_ASSERT_EQUAL_STRING("-----BEGIN CERTIFICATE-----\ntest_certificate\n-----END CERTIFICATE-----", read_settings[Settings::WEB_CERTIFICATE].c_str());
}

void run_all_sd_card_tests() {
    RUN_TEST(test_initialize_sd_card_handler);
    RUN_TEST(test_write_to_sd_card);
    RUN_TEST(test_read_from_sd_card);
    RUN_TEST(test_calculate_crc);
    RUN_TEST(test_save_and_read_settings);
}
