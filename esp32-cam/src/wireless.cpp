#include "wireless.hpp"
#include "debug.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

WirelessHandler::WirelessHandler(SDcardHandler *sdcardhandler, int wifi_retry_limit) {
    this->sdcardHandler = sdcardhandler;

    this->WIFI_RETRY_ATTEMPTS = wifi_retry_limit;
    this->wifi_retry_count = 0;

    this->netif = NULL;
    this->s_wifi_event_group = NULL;

    this->wifi_mutex = xSemaphoreCreateMutex();
}

esp_err_t WirelessHandler::init(void) {
    DEBUG("Initializing Wi-Fi");
    xSemaphoreTake(this->wifi_mutex, portMAX_DELAY);

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
        // TODO: handle error

        xSemaphoreGive(this->wifi_mutex);
        return ret;
    }

    ret = esp_event_loop_create_default();
    DEBUG("Wi-Fi event loop created");
    if (ret != ESP_OK) {
        DEBUG("Failed to create event loop: ", esp_err_to_name(ret));
        // TODO: handle error

        xSemaphoreGive(this->wifi_mutex);
        return ret;
    }

    ret = esp_wifi_set_default_wifi_sta_handlers();
    DEBUG("Wi-Fi default STA handlers set");
    if (ret != ESP_OK) {
        DEBUG("Failed to set default Wi-Fi STA handlers: ", esp_err_to_name(ret));
        // TODO: handle error

        xSemaphoreGive(this->wifi_mutex);
        return ret;
    }

    this->netif = esp_netif_create_default_wifi_sta();
    DEBUG("Wi-Fi default STA netif created");
    if (this->netif == NULL) {
        DEBUG("Failed to create default Wi-Fi STA netif");
        // TODO: handle error

        xSemaphoreGive(this->wifi_mutex);
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

    xSemaphoreGive(this->wifi_mutex);
    return ret;
}

esp_err_t WirelessHandler::connect(const char *wifi_ssid, const char *wifi_password) {
    DEBUG("Connecting to Wi-Fi network: ", wifi_ssid);
    xSemaphoreTake(this->wifi_mutex, portMAX_DELAY);

    // Initialize Wi-Fi driver if not already done
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_AUTHMODE;

    strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(this->s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE,
                                           pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        xSemaphoreGive(this->wifi_mutex);
        return ESP_OK;
    }

    // TODO: handle error

    xSemaphoreGive(this->wifi_mutex);
    DEBUG("Failed to connect to Wi-Fi network\n");
    return ESP_FAIL;
}

esp_err_t WirelessHandler::disconnect(void) {
    xSemaphoreTake(this->wifi_mutex, portMAX_DELAY);
    DEBUG("Disconnecting from Wi-Fi network");
    if (this->s_wifi_event_group) { vEventGroupDelete(this->s_wifi_event_group); }
    DEBUG("Wi-Fi network connection status: ", this->isConnected());
    xSemaphoreGive(this->wifi_mutex);
    return esp_wifi_disconnect();
}

esp_err_t WirelessHandler::deinit(void) {
    DEBUG("Deinitializing Wi-Fi");
    xSemaphoreTake(this->wifi_mutex, portMAX_DELAY);

    DEBUG("Deinitializing Wi-Fi");
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_ERR_WIFI_NOT_INIT) {
        DEBUG("Wi-Fi not initialized");
        // TODO: handle error

        xSemaphoreGive(this->wifi_mutex);
        return ret;
    }

    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(this->netif));
    esp_netif_destroy(this->netif);

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, this->ip_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, this->wifi_event_handler));

    DEBUG("Wi-Fi deinitialized\n");

    xSemaphoreGive(this->wifi_mutex);
    return ESP_OK;
}

bool WirelessHandler::isConnected(void) { return xEventGroupGetBits(this->s_wifi_event_group) & WIFI_CONNECTED_BIT; }

void WirelessHandler::ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    DEBUG("Handling IP event, event code: ", event_id);
    ip_event_got_ip_t *event_ip = nullptr;
    ip_event_got_ip6_t *event_ip6 = nullptr;

    switch (event_id) {
        case (IP_EVENT_STA_GOT_IP):
            event_ip = (ip_event_got_ip_t *)event_data;
            DEBUG("Got IP: ", IP2STR(&event_ip->ip_info.ip));
            this->wifi_retry_count = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case (IP_EVENT_STA_LOST_IP):
            DEBUG("Lost IP\n");
            break;
        case (IP_EVENT_GOT_IP6):
            event_ip6 = (ip_event_got_ip6_t *)event_data;
            DEBUG("Got IPv6: ", IPV62STR(event_ip6->ip6_info.ip));
            this->wifi_retry_count = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            DEBUG("IP event not handled");
            break;
    }
}

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
        case (WIFI_EVENT_STA_DISCONNECTED):
            DEBUG("Wi-Fi disconnected");
            break;
        case (WIFI_EVENT_STA_AUTHMODE_CHANGE):
            DEBUG("Wi-Fi authmode changed");
            break;
        default:
            DEBUG("Wi-Fi event not handled");
            break;
    }
}

const char *WirelessHandler::get_setting(Settings settingID) {
    return this->settings[settingID].c_str();
}

int WirelessHandler::set_setting(const char *buffer, const size_t buffer_len, Settings settingID) {
    this->settings[settingID] = std::string(buffer, buffer_len);
    return 0;
}

bool WirelessHandler::set_all_settings(std::map <Settings, std::string> settings) {
    this->settings = settings;
    return true;
}

int WirelessHandler::save_settings_to_sdcard(std::map <Settings, std::string> settings) {
    return this->sdcardHandler->save_all_settings(settings);
}

int WirelessHandler::read_settings_from_sdcard(std::map <Settings, std::string> settings) {
    return this->sdcardHandler->read_all_settings(settings);
}
