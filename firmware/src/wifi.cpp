/**
 * JARVIS Device — WiFi Manager
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <nvs.h>

#include "config.h"
#include "states.h"

static const char* TAG = "wifi";

extern connection_status_t g_conn;

// ============================================================================
// Event Handler
// ============================================================================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, connecting to SSID: %s", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected");
        g_conn.wifi_connected = 1;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        g_conn.wifi_connected = 0;
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_conn.wifi_connected = 1;
    }
}

// ============================================================================
// Init WiFi
// ============================================================================
void init_wifi(void) {
    ESP_LOGI(TAG, "Initializing WiFi...");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Try to load saved credentials from NVS
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &nvs);
    
    char saved_ssid[64] = {0};
    char saved_pass[64] = {0};
    size_t ssid_len = sizeof(saved_ssid);
    size_t pass_len = sizeof(saved_pass);
    
    if (err == ESP_OK) {
        nvs_get_str(nvs, "ssid", saved_ssid, &ssid_len);
        nvs_get_str(nvs, "pass", saved_pass, &pass_len);
        nvs_close(nvs);
        
        if (strlen(saved_ssid) > 0) {
            ESP_LOGI(TAG, "Loaded WiFi credentials from NVS: SSID=%s", saved_ssid);
        }
    }

    // Use saved credentials if available, otherwise use compile-time defaults
    const char* ssid = strlen(saved_ssid) > 0 ? saved_ssid : WIFI_SSID;
    const char* pass = strlen(saved_pass) > 0 ? saved_pass : WIFI_PASSWORD;

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init complete, connecting to %s...", ssid);
}

// ============================================================================
// Save WiFi credentials to NVS
// ============================================================================
void wifi_save_credentials(const char* ssid, const char* pass) {
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "pass", pass));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    ESP_LOGI(TAG, "WiFi credentials saved to NVS");
}
