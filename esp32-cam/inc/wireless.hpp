#ifndef WIRELESS_HPP
#define WIRELESS_HPP

#include <inttypes.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "testMacros.hpp"

#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

class WirelessHandler {
    public:
        WirelessHandler(const char* ssid = WIFI_SSID, const char* password = WIFI_SSID, int wifi_retry_limit = 3);
        esp_err_t init(void);
        esp_err_t connect(const char *ssid, const char *password);
        esp_err_t disconnect(void);
        esp_err_t deinit(void);
        bool isConnected(void);


    private:
        static void ip_event_cb_lambda(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
            auto *self = static_cast<WirelessHandler*>(arg);
            self->ip_event_cb(arg, event_base, event_id, event_data);
        }
        void ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

        static void wifi_event_cb_lambda(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
            auto *self = static_cast<WirelessHandler*>(arg);
            self->wifi_event_cb(arg, event_base, event_id, event_data);
        }
        void wifi_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);   

        const char* ssid;
        const char* password;
        int WIFI_RETRY_ATTEMPTS;
        int wifi_retry_count;
        esp_netif_t *netif;
        esp_event_handler_instance_t ip_event_handler;
        esp_event_handler_instance_t wifi_event_handler;
        EventGroupHandle_t s_wifi_event_group;
};

#endif