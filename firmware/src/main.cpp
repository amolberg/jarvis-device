/**
 * JARVIS Device — Main Entry Point
 * 
 * ESP32-S3 firmware for JARVIS room nodes.
 * Handles: WiFi, MQTT, I2S audio capture, LED ring, HTTP audio streaming to JARVIS Core.
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

#include "config.h"
#include "states.h"

static const char* TAG = "jarvis-device";

// ============================================================================
// Forward declarations
// ============================================================================
void init_nvs(void);
void init_wifi(void);
void init_mqtt(void);
void init_audio(void);
void init_led(void);
void init_http_stream(void);

void wifi_task(void* param);
void mqtt_task(void* param);
void audio_task(void* param);
void led_task(void* param);
void http_stream_task(void* param);
void state_machine_task(void* param);
void heartbeat_task(void* param);

void set_device_state(device_state_t state);
void set_led_mode(led_mode_t mode);
void publish_status(void);

// ============================================================================
// Global state
// ============================================================================
static device_state_t g_device_state = DEVICE_STATE_INIT;
static led_mode_t g_led_mode = LED_MODE_OFF;
static SemaphoreHandle_t g_state_mutex;
static QueueHandle_t g_audio_queue;
static QueueHandle_t g_cmd_queue;
static TaskHandle_t g_audio_task_handle = NULL;
static TaskHandle_t g_http_stream_handle = NULL;
static bool g_is_streaming = false;
static uint32_t g_stream_start_ms = 0;

// Connection status
static connection_status_t g_conn = {0};

// ============================================================================
// Entry Point
// ============================================================================
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, " JARVIS Device v1.0.0 — ESP32-S3");
    ESP_LOGI(TAG, " Build: %s %s", __DATE__, __TIME__);
    ESP_LOGI(TAG, "===========================================");

    // Init NVS (required for WiFi)
    init_nvs();

    // Create synchronization primitives
    g_state_mutex = xSemaphoreCreateMutex();
    g_audio_queue = xQueueCreate(8, sizeof(audio_chunk_t));
    g_cmd_queue = xQueueCreate(16, sizeof(mqtt_cmd_t));

    if (!g_state_mutex || !g_audio_queue || !g_cmd_queue) {
        ESP_LOGE(TAG, "Failed to create queues/mutex");
        esp_restart();
    }

    // Set LED to init mode while we start up
    set_led_mode(LED_MODE_OFF);

    // Init subsystems
    init_wifi();
    init_audio();
    init_led();

    // Set state
    set_device_state(DEVICE_STATE_CONNECTING);
    set_led_mode(LED_MODE_PULSE);

    // Init MQTT after WiFi connects
    init_mqtt();

    // Init HTTP streaming client
    init_http_stream();

    // Start core tasks
    xTaskCreatePinnedToCore(&led_task, "led_task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(&state_machine_task, "state_machine", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(&heartbeat_task, "heartbeat", 2048, NULL, 1, NULL, 1);

    // Start audio capture task (starts in IDLE mode, not streaming)
    xTaskCreatePinnedToCore(&audio_task, "audio_task", 8192, NULL, 4, &g_audio_task_handle, 0);

    ESP_LOGI(TAG, "All subsystems initialized. JARVIS device ready.");
    set_device_state(DEVICE_STATE_IDLE);
    set_led_mode(LED_MODE_PULSE);
}

// ============================================================================
// State Management
// ============================================================================
void set_device_state(device_state_t state) {
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    if (g_device_state != state) {
        ESP_LOGI(TAG, "State: %d → %d", g_device_state, state);
        g_device_state = state;
    }
    xSemaphoreGive(g_state_mutex);
}

device_state_t get_device_state(void) {
    device_state_t state;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    state = g_device_state;
    xSemaphoreGive(g_state_mutex);
    return state;
}

void set_led_mode(led_mode_t mode) {
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_led_mode = mode;
    xSemaphoreGive(g_state_mutex);
}

// ============================================================================
// NVS Initialization
// ============================================================================
void init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash full, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "NVS initialized");
}

// ============================================================================
// Heartbeat — publishes status to MQTT every 60s
// ============================================================================
void heartbeat_task(void* param) {
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(60000));
        
        device_state_t st = get_device_state();
        ESP_LOGD(TAG, "Heartbeat: state=%d wifi=%d mqtt=%d jarvis=%d",
                 st, g_conn.wifi_connected, g_conn.mqtt_connected, g_conn.jarvis_reachable);
        
        publish_status();
    }
}

void publish_status(void) {
    // Status is published via MQTT in mqtt_task
    // This function is called by heartbeat
}

// ============================================================================
// State Machine Task — processes MQTT commands and coordinates subsystems
// ============================================================================
void state_machine_task(void* param) {
    mqtt_cmd_t cmd;
    
    while (true) {
        if (xQueueReceive(g_cmd_queue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE) {
            device_state_t st = get_device_state();
            
            switch (cmd) {
                case MQTT_CMD_SLEEP:
                    ESP_LOGI(TAG, "Command: SLEEP");
                    set_device_state(DEVICE_STATE_SLEEPING);
                    set_led_mode(LED_MODE_OFF);
                    break;
                    
                case MQTT_CMD_WAKE:
                    ESP_LOGI(TAG, "Command: WAKE");
                    set_device_state(DEVICE_STATE_IDLE);
                    set_led_mode(LED_MODE_PULSE);
                    break;
                    
                case MQTT_CMD_STATUS:
                    ESP_LOGI(TAG, "Command: STATUS request");
                    // Publish status via MQTT
                    break;
                    
                case MQTT_CMD_RESET:
                    ESP_LOGI(TAG, "Command: RESET");
                    esp_restart();
                    break;
                    
                case MQTT_CMD_OTA_CHECK:
                case MQTT_CMD_OTA_START:
                    ESP_LOGI(TAG, "Command: OTA");
                    // Trigger OTA update
                    break;
                    
                case MQTT_CMD_SET_LED:
                case MQTT_CMD_SET_VOLUME:
                    // These are handled with data in the MQTT callback
                    break;
                    
                default:
                    break;
            }
        }
        
        // Monitor audio queue for voice activity
        audio_chunk_t chunk;
        if (xQueueReceive(g_audio_queue, &chunk, 0) == pdTRUE) {
            if (chunk.vad_active && !g_is_streaming) {
                device_state_t st = get_device_state();
                if (st == DEVICE_STATE_IDLE || st == DEVICE_STATE_AUDIO_READY) {
                    ESP_LOGI(TAG, "VAD: Voice detected — starting stream");
                    g_is_streaming = true;
                    g_stream_start_ms = esp_log_timestamp();
                    set_device_state(DEVICE_STATE_STREAMING);
                    set_led_mode(LED_MODE_LISTEN);
                    
                    // Signal HTTP stream task to start
                    if (g_http_stream_handle) {
                        xTaskNotifyGive(g_http_stream_handle);
                    }
                }
            }
            
            // Put chunk back for HTTP stream (peek pattern)
            // Actually just re-queue for the HTTP task to pick up
            if (g_is_streaming) {
                xQueueSend(g_audio_queue, &chunk, 0);
            }
            
            // Check for silence
            if (chunk.silence_frame && g_is_streaming) {
                static int silence_count = 0;
                silence_count++;
                if (silence_count >= VAD_SILENCE_CHUNKS) {
                    ESP_LOGI(TAG, "VAD: Silence detected — stopping stream");
                    silence_count = 0;
                    g_is_streaming = false;
                    set_device_state(DEVICE_STATE_PROCESSING);
                    set_led_mode(LED_MODE_ACTIVE);
                    
                    // Notify HTTP stream to flush
                    if (g_http_stream_handle) {
                        xTaskNotifyGive(g_http_stream_handle);
                    }
                }
            } else {
                // Reset silence counter if we got audio
                // (handled in audio_task)
            }
        }
    }
}

// ============================================================================
// Placeholder stubs — implemented in separate files
// ============================================================================
void init_wifi(void) { /* wifi.cpp */ }
void init_mqtt(void) { /* mqtt.cpp */ }
void init_audio(void) { /* audio.cpp */ }
void init_led(void) { /* led.cpp */ }
void init_http_stream(void) { /* http_stream.cpp */ }
void led_task(void* param) { /* led.cpp */ }
void audio_task(void* param) { /* audio.cpp */ }
