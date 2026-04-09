/**
 * JARVIS Device — MQTT Client
 * 
 * Connects to Home Assistant MQTT broker.
 * Subscribes to JARVIS command topics and publishes device status.
 * Enables remote control of the device without needing direct HTTP access.
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>

// MQTT (using ESP-MQTT from esp-idf)
#include <mqtt_client.h>

#include "config.h"
#include "states.h"

static const char* TAG = "mqtt";

extern esp_mqtt_client_handle_t g_mqtt_client;
extern QueueHandle_t g_cmd_queue;
extern connection_status_t g_conn;
extern device_state_t get_device_state(void);
extern void set_led_mode(led_mode_t mode);
extern void set_device_state(device_state_t state);

// MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

// Device ID (unique per device)
static char device_id[32] = {0};
static char cmd_topic[64];
static char state_topic[64];
static char avail_topic[64];

// ============================================================================
// Generate unique device ID from MAC address
// ============================================================================
static void generate_device_id(void) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(device_id, sizeof(device_id), "jarvis-%02x%02x%02x", mac[3], mac[4], mac[5]);
    snprintf(cmd_topic, sizeof(cmd_topic), "home/jarvis-device/%s/cmd", device_id);
    snprintf(state_topic, sizeof(state_topic), "home/jarvis-device/%s/state", device_id);
    snprintf(avail_topic, sizeof(avail_topic), "home/jarvis-device/%s/avail", device_id);
    ESP_LOGI(TAG, "Device ID: %s", device_id);
}

// ============================================================================
// Publish device availability (online/offline)
// ============================================================================
static void publish_availability(bool online) {
    if (mqtt_client && mqtt_connected) {
        esp_mqtt_client_publish(mqtt_client, avail_topic, 
            online ? "online" : "offline", 0, 1, 1);
    }
}

// ============================================================================
// Publish device state to MQTT (for Home Assistant auto-discovery)
// ============================================================================
static void publish_state(void) {
    if (!mqtt_client || !mqtt_connected) return;

    device_state_t st = get_device_state();
    const char* state_str;
    switch (st) {
        case DEVICE_STATE_IDLE:       state_str = "idle"; break;
        case DEVICE_STATE_STREAMING:  state_str = "streaming"; break;
        case DEVICE_STATE_PROCESSING:  state_str = "processing"; break;
        case DEVICE_STATE_SPEAKING:   state_str = "speaking"; break;
        case DEVICE_STATE_SLEEPING:   state_str = "sleeping"; break;
        case DEVICE_STATE_ERROR:      state_str = "error"; break;
        case DEVICE_STATE_INIT:
        case DEVICE_STATE_CONNECTING:  state_str = "booting"; break;
        default:                      state_str = "unknown"; break;
    }

    char payload[256];
    int len = snprintf(payload, sizeof(payload),
        R"({"state":"%s","device":"%s","rssi":%d,"uptime":%lu})",
        state_str, device_id, 
        0,  // RSSI would need wifi info
        (unsigned long)(esp_log_timestamp() / 1000)
    );

    esp_mqtt_client_publish(mqtt_client, state_topic, payload, len, 1, 0);
}

// ============================================================================
// Home Assistant Auto-Discovery
// ============================================================================
static void publish_discovery(void) {
    if (!mqtt_client || !mqtt_connected) return;

    // Home Assistant MQTT discovery for a sensor
    char discovery_topic[128];
    char discovery_payload[512];

    snprintf(discovery_topic, sizeof(discovery_topic),
        "homeassistant/sensor/%s/config", device_id);

    snprintf(discovery_payload, sizeof(discovery_payload),
        R"({
            "name":"JARVIS Device Status",
            "unique_id":"%s_status",
            "state_topic":"%s",
            "device":{
                "identifiers":["%s"],
                "name":"JARVIS Room Device",
                "model":"ESP32-S3 DIY Node",
                "manufacturer":"Zebratic"
            },
            "json_attributes_topic":"%s"
        })",
        device_id, state_topic, device_id, state_topic
    );

    esp_mqtt_client_publish(mqtt_client, discovery_topic, discovery_payload, 0, 1, 1);
    ESP_LOGI(TAG, "HA discovery published");
}

// ============================================================================
// Parse incoming MQTT command
// ============================================================================
static void handle_command(const char* payload, int len) {
    // Parse JSON command
    // Expected format: {"cmd":"sleep"|"wake"|"ota"|"status"|"reset", ...}
    
    // Simple string matching (no JSON parsing overhead)
    if (strstr(payload, "\"cmd\":\"sleep\"") || strstr(payload, "\"cmd\": \"sleep\"")) {
        mqtt_cmd_t cmd = MQTT_CMD_SLEEP;
        xQueueSend(g_cmd_queue, &cmd, 0);
    } else if (strstr(payload, "\"cmd\":\"wake\"") || strstr(payload, "\"cmd\": \"wake\"")) {
        mqtt_cmd_t cmd = MQTT_CMD_WAKE;
        xQueueSend(g_cmd_queue, &cmd, 0);
    } else if (strstr(payload, "\"cmd\":\"reset\"") || strstr(payload, "\"cmd\": \"reset\"")) {
        mqtt_cmd_t cmd = MQTT_CMD_RESET;
        xQueueSend(g_cmd_queue, &cmd, 0);
    } else if (strstr(payload, "\"cmd\":\"ota\"") || strstr(payload, "\"cmd\": \"ota\"")) {
        mqtt_cmd_t cmd = MQTT_CMD_OTA_CHECK;
        xQueueSend(g_cmd_queue, &cmd, 0);
    } else if (strstr(payload, "\"cmd\":\"status\"") || strstr(payload, "\"cmd\": \"status\"")) {
        publish_state();
    }
}

// ============================================================================
// MQTT Event Handler
// ============================================================================
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, 
                               int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            mqtt_connected = true;
            g_conn.mqtt_connected = 1;
            
            publish_availability(true);
            publish_discovery();
            
            // Subscribe to command topic
            int msg_id = esp_mqtt_client_subscribe(mqtt_client, cmd_topic, 1);
            ESP_LOGI(TAG, "Subscribed to %s (msg_id=%d)", cmd_topic, msg_id);
            
            // Subscribe to global jarvis commands
            msg_id = esp_mqtt_client_subscribe(mqtt_client, "home/jarvis-device/+/cmd", 1);
            ESP_LOGI(TAG, "Subscribed to global cmd (msg_id=%d)", msg_id);
            
            publish_state();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            mqtt_connected = false;
            g_conn.mqtt_connected = 0;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT subscribed: msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT unsubscribed: msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT published: msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT DATA: topic=%.*s", event->topic_len, event->topic);
            
            if (event->data_len > 0) {
                char topic_str[128] = {0};
                char data_str[512] = {0};
                int copy_len = event->topic_len < 127 ? event->topic_len : 127;
                memcpy(topic_str, event->topic, copy_len);
                copy_len = event->data_len < 511 ? event->data_len : 511;
                memcpy(data_str, event->data, copy_len);
                
                ESP_LOGD(TAG, "  data: %s", data_str);
                handle_command(data_str, strlen(data_str));
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        default:
            break;
    }
}

// ============================================================================
// MQTT Init
// ============================================================================
void init_mqtt(void) {
    generate_device_id();

    char mqtt_uri[128];
    snprintf(mqtt_uri, sizeof(mqtt_uri), 
             "mqtt://%s:%d", MQTT_SERVER, MQTT_PORT);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_uri,
        .credentials.username = MQTT_USERNAME[0] ? MQTT_USERNAME : NULL,
        .credentials.authentication.password = MQTT_PASSWORD[0] ? MQTT_PASSWORD : NULL,
        .session.keepalive = 60,
        .session.last_will.topic = avail_topic,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    // If username is empty string, set to NULL
    if (strlen(MQTT_USERNAME) == 0) mqtt_cfg.credentials.username = NULL;
    if (strlen(MQTT_PASSWORD) == 0) mqtt_cfg.credentials.authentication.password = NULL;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, 
                                                    mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
    
    ESP_LOGI(TAG, "MQTT client starting: %s", mqtt_uri);
}

// Getter for external access
esp_mqtt_client_handle_t get_mqtt_client(void) { return mqtt_client; }
