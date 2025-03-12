#include "wireless.hpp"
#include "debug.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "scopedMutex.hpp"
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <unordered_map>


#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/**
 * @brief Constructor for the WirelessHandler class.
 * 
 * This constructor initializes the WirelessHandler instance, setting up the
 * SDcardHandler, retry attempts for WiFi connection, and other internal variables.
 * It also creates a mutex for managing concurrent access to the wireless resources.
 * 
 * @param sdcardhandler A shared pointer to an SDcardHandler instance used for handling SD card operations.
 * 
 * @return void
 */
WirelessHandler::WirelessHandler(std::shared_ptr<SDcardHandler> sdcardhandler) {
    this->sdcardHandler = sdcardhandler;

    this->WIFI_RETRY_ATTEMPTS = WIFI_RETRY_LIMIT;

    this->netif = NULL;
    this->s_wifi_event_group = NULL;

    this->wifi_mutex = xSemaphoreCreateMutex();
}

/**
 * @brief Initializes the Wi-Fi stack and related resources.
 * 
 * This method sets up the necessary components for Wi-Fi functionality, including:
 * - Initializing Non-Volatile Storage (NVS) for Wi-Fi configuration.
 * - Creating the Wi-Fi event group and event loop.
 * - Setting default Wi-Fi handlers and creating a default Wi-Fi station network interface.
 * - Configuring the Wi-Fi stack and registering event handlers for Wi-Fi and IP events.
 * - Setting a custom hostname for the device.
 * 
 * @return esp_err_t Returns ESP_OK on success or an error code on failure.
 */
esp_err_t WirelessHandler::init(void) {
    DEBUG("Initializing Wi-Fi");
    ScopedMutex lock(this->wifi_mutex);

    // Initialize Non-Volatile Storage
    DEBUG("Initializing NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    this->s_wifi_event_group = xEventGroupCreate();

    ret = esp_netif_init();
    DEBUG("Wi-Fi netif initialized");
    if (ret != ESP_OK) {
        DEBUG("Failed to initialize netif: ", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    DEBUG("Wi-Fi event loop created");
    if (ret != ESP_OK) {
        DEBUG("Failed to create event loop: ", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_default_wifi_sta_handlers();
    DEBUG("Wi-Fi default STA handlers set");
    if (ret != ESP_OK) {
        DEBUG("Failed to set default Wi-Fi STA handlers: ", esp_err_to_name(ret));
        return ret;
    }

    this->netif = esp_netif_create_default_wifi_sta();
    DEBUG("Wi-Fi default STA netif created");
    if (this->netif == nullptr) {
        DEBUG("Failed to create default Wi-Fi STA netif");
        return ESP_FAIL;
    }

    // Set hostname
    const char *hostname = "Stargazer";
    esp_err_t err = esp_netif_set_hostname(netif, hostname);
    if (err == ESP_OK) {
        DEBUG("Hostname set to: ", hostname);
    } else {
        DEBUG("Failed to set hostname: ", esp_err_to_name(err));
    }

    // Wi-Fi stack configuration parameters
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WirelessHandler::wifi_event_cb_lambda, this, &this->wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &WirelessHandler::ip_event_cb_lambda, this, &this->ip_event_handler));

    return ret;
}

/**
 * @brief Connects to a Wi-Fi network using the provided SSID and password.
 * 
 * This method attempts to connect to the specified Wi-Fi network by:
 * - Configuring the Wi-Fi credentials (SSID and password).
 * - Setting the Wi-Fi mode to Station (STA).
 * - Starting the Wi-Fi connection process.
 * - Retries the connection a predefined number of times (based on `WIFI_RETRY_ATTEMPTS`), waiting for a connection event.
 * 
 * The method will return:
 * - `ESP_OK` if the connection is successfully established.
 * - `ESP_FAIL` if the connection fails after the maximum retry attempts.
 * 
 * @param wifi_ssid The SSID of the Wi-Fi network to connect to.
 * @param wifi_password The password for the Wi-Fi network.
 * 
 * @return esp_err_t Returns `ESP_OK` if connected successfully, or an error code (`ESP_FAIL`) if the connection fails.
 */
esp_err_t WirelessHandler::connect(const char *wifi_ssid, const char *wifi_password) {
    ScopedMutex lock(this->wifi_mutex);
    DEBUG("Connecting to Wi-Fi network: ", wifi_ssid);

    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_AUTHMODE;
    strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    int retries = 0;
    while (retries < this->WIFI_RETRY_ATTEMPTS) {
        if (xEventGroupWaitBits(this->s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(5000))) {
            DEBUG("Wi-Fi Connected!");
            return ESP_OK;
        }
        DEBUG("Retrying Wi-Fi connection...");
        esp_wifi_disconnect();
        esp_wifi_connect();
        retries++;
    }

    DEBUG("Wi-Fi connection failed after ", retries, " attempts.");
    return ESP_FAIL;
}

/**
 * @brief Disconnects from the current Wi-Fi network.
 * 
 * This method attempts to disconnect from the currently connected Wi-Fi network by:
 * - Calling `esp_wifi_disconnect()` to initiate the disconnection.
 * - Deleting the Wi-Fi event group used for connection status.
 * 
 * If disconnection is successful, it returns the result of the `esp_wifi_disconnect()` call.
 * If the disconnection fails, an error message is logged with the corresponding error code.
 * 
 * @return esp_err_t Returns `ESP_OK` if disconnection is successful, or an error code if it fails.
 */
esp_err_t WirelessHandler::disconnect(void) {
    ScopedMutex lock(this->wifi_mutex);
    DEBUG("Disconnecting from Wi-Fi network");

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) { DEBUG("Failed to disconnect: ", esp_err_to_name(err)); }

    if (this->s_wifi_event_group) {
        vEventGroupDelete(this->s_wifi_event_group);
        this->s_wifi_event_group = NULL;
    }

    return err;
}

/**
 * @brief Deinitializes the Wi-Fi subsystem.
 * 
 * This method stops the Wi-Fi driver, deinitializes the Wi-Fi subsystem, and clears the default Wi-Fi driver and handlers.
 * It also destroys the Wi-Fi network interface and unregisters any event handlers related to Wi-Fi and IP events.
 * 
 * The function performs the following steps:
 * - Stops the Wi-Fi driver using `esp_wifi_stop()`.
 * - Deinitializes the Wi-Fi driver and clears default Wi-Fi handlers.
 * - Destroys the network interface.
 * - Unregisters event handlers for IP and Wi-Fi events.
 * 
 * @return esp_err_t Returns `ESP_OK` if the Wi-Fi subsystem is deinitialized successfully, or an error code if any operation fails.
 */
esp_err_t WirelessHandler::deinit(void) {
    ScopedMutex lock(this->wifi_mutex);
    DEBUG("Deinitializing Wi-Fi");

    DEBUG("Deinitializing Wi-Fi");
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_ERR_WIFI_NOT_INIT) {
        DEBUG("Wi-Fi not initialized");
        return ret;
    }

    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(this->netif));
    esp_netif_destroy(this->netif);

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, this->ip_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, this->wifi_event_handler));

    DEBUG("Wi-Fi deinitialized\n");
    return ESP_OK;
}

/**
 * @brief Checks if the device is connected to a Wi-Fi network.
 * 
 * This method checks the status of the Wi-Fi connection by examining the Wi-Fi event group.
 * It returns `true` if the device is connected to a Wi-Fi network, and `false` otherwise.
 * 
 * @return bool Returns `true` if the device is connected to Wi-Fi, otherwise `false`.
 */
bool WirelessHandler::isConnected(void) {
    return this->s_wifi_event_group && (xEventGroupGetBits(this->s_wifi_event_group) & WIFI_CONNECTED_BIT);
}

/**
 * @brief Callback function to handle IP-related events.
 * 
 * This function handles various IP events such as obtaining an IPv4 or IPv6 address,
 * and losing an IP address. It processes the IP event and updates the Wi-Fi event group 
 * accordingly. It also logs information about the event, including the IP addresses.
 * 
 * @param arg A pointer to the argument passed during the event registration. Typically unused.
 * @param event_base The event base that triggered this callback.
 * @param event_id The specific event ID associated with the IP event (e.g., IP_EVENT_STA_GOT_IP, IP_EVENT_STA_LOST_IP, IP_EVENT_GOT_IP6).
 * @param event_data A pointer to the event-specific data (contains IP information).
 * 
 * @note This function updates the Wi-Fi event group to indicate whether the device is connected or lost the connection.
 */
void WirelessHandler::ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    DEBUG("Handling IP event, event code: ", event_id);
    ip_event_got_ip_t *event_ip = nullptr;
    ip_event_got_ip6_t *event_ip6 = nullptr;

    switch (event_id) {
        case (IP_EVENT_STA_GOT_IP):
            event_ip = (ip_event_got_ip_t *)event_data;
            DEBUG("Got IP: ", IP2STR(&event_ip->ip_info.ip));
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case (IP_EVENT_STA_LOST_IP):
            DEBUG("Lost IP\n");
            break;
        case (IP_EVENT_GOT_IP6):
            event_ip6 = (ip_event_got_ip6_t *)event_data;
            DEBUG("Got IPv6: ", IPV62STR(event_ip6->ip6_info.ip));
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            DEBUG("IP event not handled");
            break;
    }
}

/**
 * @brief Callback function to handle Wi-Fi related events.
 * 
 * This function handles various Wi-Fi events, including Wi-Fi initialization, connection attempts, 
 * connection status changes, and authentication mode changes. It processes the event and logs the relevant
 * information, and takes actions such as retrying the connection or attempting to reconnect.
 * 
 * @param arg A pointer to the argument passed during the event registration. Typically unused.
 * @param event_base The event base that triggered this callback.
 * @param event_id The specific event ID associated with the Wi-Fi event (e.g., WIFI_EVENT_STA_CONNECTED, WIFI_EVENT_STA_DISCONNECTED).
 * @param event_data A pointer to the event-specific data (contains Wi-Fi event information).
 * 
 * @note This function also handles the Wi-Fi reconnection logic in case of disconnections.
 */
void WirelessHandler::wifi_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    DEBUG("Handling Wi-Fi event, event code: ", event_id);

    switch (event_id) {
        case (WIFI_EVENT_WIFI_READY):
            DEBUG("Wi-Fi ready");
            break;
        case (WIFI_EVENT_SCAN_DONE):
            DEBUG("Wi-Fi scan done");
            break;
        case (WIFI_EVENT_STA_START):
            DEBUG("Wi-Fi started, connecting to AP...");
            esp_wifi_connect();
            break;
        case (WIFI_EVENT_STA_STOP):
            DEBUG("Wi-Fi stopped");
            break;
        case (WIFI_EVENT_STA_CONNECTED):
            DEBUG("Wi-Fi connected");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            DEBUG("Wi-Fi disconnected, retrying...");
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            if (this->WIFI_RETRY_ATTEMPTS > 0) { esp_wifi_connect(); }
            break;

        case (WIFI_EVENT_STA_AUTHMODE_CHANGE):
            DEBUG("Wi-Fi authmode changed");
            break;
        default:
            DEBUG("Wi-Fi event not handled");
            break;
    }
}

/**
 * @brief Retrieves the value of a specific setting.
 * 
 * This function returns the value of the specified setting as a constant C-string. The setting is 
 * identified by the provided `settingID`. The settings are stored internally in a container, and this 
 * function retrieves the value associated with the given setting ID.
 * 
 * @param settingID The ID of the setting to retrieve.
 * @return A pointer to a constant C-string representing the value of the requested setting.
 * 
 * @note The returned string is a reference to an internal value, so its lifetime is tied to the 
 *       lifespan of the `WirelessHandler` instance.
 */
const char *WirelessHandler::get_setting(Settings settingID) { return this->settings[settingID].c_str(); }

/**
 * @brief Retrieves a pointer to the container holding all settings.
 * 
 * This function returns a pointer to the internal `std::unordered_map` that stores all settings.
 * The map associates `Settings` keys with their corresponding values stored as `std::string`.
 * 
 * @return A pointer to the `std::unordered_map` containing all settings.
 * 
 * @note Modifying the returned map directly will affect the internal settings of the `WirelessHandler` instance.
 */
std::unordered_map<Settings, std::string>* WirelessHandler::get_all_settings_pointer() { return &this->settings; }

/**
 * @brief Sets a setting value for a specified setting ID.
 * 
 * This function assigns the provided buffer content to the setting specified by `settingID`. 
 * It copies `buffer_len` characters from the given buffer into the corresponding setting value. 
 * If the buffer is invalid (null or empty), the operation is aborted and returns an error.
 * 
 * @param buffer A pointer to the buffer containing the setting value to be set.
 * @param buffer_len The length of the buffer (number of characters) to be copied.
 * @param settingID The ID of the setting to be updated.
 * 
 * @return `0` on success, or `-1` if the buffer is invalid (null or empty).
 */
int WirelessHandler::set_setting(const char *buffer, const size_t buffer_len, Settings settingID) {
    if (buffer == nullptr || buffer_len == 0) return -1; // Prevent invalid writes
    this->settings[settingID] = std::string(buffer, buffer_len);
    return 0;
}

/**
 * @brief Sets multiple settings at once by updating the internal settings map.
 * 
 * This function updates all settings by replacing the current settings map with the 
 * provided one. The `settings` map must contain key-value pairs where the keys are 
 * `Settings` IDs and the values are the corresponding setting values as strings.
 * 
 * @param settings A map of settings to be set, where keys are setting IDs and values are setting values.
 * 
 * @return `true` if the settings were successfully updated.
 */
bool WirelessHandler::set_all_settings(std::unordered_map<Settings, std::string> settings) {
    this->settings = settings;
    return true;
}

/**
 * @brief Saves the provided settings to the SD card.
 * 
 * This function delegates the task of saving the settings to the `sdcardHandler`'s 
 * `save_all_settings` function. The provided `settings` map will be written to the SD card.
 * 
 * @param settings A map of settings to be saved, where keys are setting IDs and values are setting values.
 * 
 * @return The result of the `save_all_settings` function from the `sdcardHandler`. 
 *         A non-negative value typically indicates success, while a negative value indicates failure.
 */
int WirelessHandler::save_settings_to_sdcard(std::unordered_map<Settings, std::string> settings) {
    return this->sdcardHandler->save_all_settings(settings);
}

/**
 * @brief Reads settings from the SD card and stores them in the provided map.
 * 
 * This function delegates the task of reading settings from the SD card to the `sdcardHandler`'s 
 * `read_all_settings` function. The provided `settings` map will be populated with the settings 
 * read from the SD card.
 * 
 * @param settings A map to store the read settings, where keys are setting IDs and values are setting values.
 * 
 * @return The result of the `read_all_settings` function from the `sdcardHandler`. 
 *         A non-negative value typically indicates success, while a negative value indicates failure.
 */
int WirelessHandler::read_settings_from_sdcard(std::unordered_map<Settings, std::string> settings) {
    return this->sdcardHandler->read_all_settings(settings);
}
