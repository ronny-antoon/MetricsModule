#pragma once

#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/ip4_addr.h>
#include <nvs_flash.h>

static void event_handler(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        ESP_LOGI("wifi_initer", "Connecting to the AP");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
        ESP_LOGI("wifi_initer", "Retrying connection to the AP");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // ip_event_got_ip_t * event = (ip_event_got_ip_t *) event_data;
        // const ip4_addr_t * addr   = (const ip4_addr_t *) (&event->ip_info.ip);
        // char * ip                 = ip4addr_ntoa(addr);
        // ESP_LOGI("wifi_initer", "Got IP: %s", ip);

        bool * wifiConnected = (bool *) arg;
        *wifiConnected       = true;
    }
}

void wifi_init_sta(bool * wifiConnected)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, wifiConnected, &instance_any_id));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, wifiConnected, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = { .ssid        = "ronnysiwar",
                 .password    = "Ronny123RRR",
                 .scan_method = WIFI_ALL_CHANNEL_SCAN,
                 .sort_method  = WIFI_CONNECT_AP_BY_SIGNAL },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("wifi_initer", "wifi_init_sta finished.");
}