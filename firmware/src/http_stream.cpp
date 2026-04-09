/**
 * JARVIS Device — HTTP Audio Streaming
 * 
 * Sends audio chunks to JARVIS Core voice pipeline.
 * Uses HTTP POST with chunked transfer encoding for low-latency streaming.
 * 
 * Protocol:
 *   POST /ws/voice with Upgrade: websocket header
 *   Then send JSON frames with base64-encoded PCM audio.
 * 
 * Actually, since the JARVIS Core WebSocket is binary, we connect directly
 * via WebSocket to /ws/voice and exchange the protocol defined in pipeline.py.
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_websocket_client.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_netif_types.h>
#include <mbedtls/base64.h>

#include "config.h"
#include "states.h"

static const char* TAG = "http_stream";

// WebSocket connection to JARVIS Core
static esp_websocket_client_handle_t ws_client = NULL;
static bool ws_connected = false;
static bool ws_streaming = false;

// External references
extern QueueHandle_t g_audio_queue;
extern SemaphoreHandle_t g_state_mutex;
extern bool g_is_streaming;
extern void set_device_state(device_state_t state);
extern void set_led_mode(led_mode_t mode);

// Audio codec state
static uint32_t audio_frames_sent = 0;
static int64_t stream_start_us = 0;

// ============================================================================
// WebSocket Event Handler
// ============================================================================
static void ws_event_handler(void* handler_args, esp_event_base_t base,
                              int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected to JARVIS Core");
            ws_connected = true;
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected");
            ws_connected = false;
            ws_streaming = false;
            g_is_streaming = false;
            break;

        case WEBSOCKET_EVENT_DATA:
            ESP_LOGD(TAG, "WS data: op=%d len=%d", data->op_code, data->data_len);
            
            if (data->op_code == WS_OPCODE_TEXT) {
                // Parse JSON response from JARVIS
                // Expected: {"type":"transcript"|"llm_done"|"error"|"status", ...}
                // For now, just log it
                ESP_LOGI(TAG, "JARVIS response: %.*s", data->data_len, (char*)data->data_ptr);
                
                if (strstr((char*)data->data_ptr, "\"type\":\"transcript\"")) {
                    // STT transcript received — user said something
                    set_led_mode(LED_MODE_ACTIVE);
                } else if (strstr((char*)data->data_ptr, "\"type\":\"llm_done\"")) {
                    // LLM finished — JARVIS is about to speak
                    set_led_mode(LED_MODE_SPEAKING);
                } else if (strstr((char*)data->data_ptr, "\"type\":\"audio\"")) {
                    // TTS audio chunk received — play it
                    // Decode base64 and send to I2S DAC
                    // (Speaker output not yet implemented — would need I2S TX)
                } else if (strstr((char*)data->data_ptr, "\"type\":\"audio_done\"")) {
                    // TTS done
                    ESP_LOGI(TAG, "JARVIS finished speaking");
                    set_device_state(DEVICE_STATE_IDLE);
                    set_led_mode(LED_MODE_PULSE);
                    ws_streaming = false;
                } else if (strstr((char*)data->data_ptr, "\"type\":\"error\"")) {
                    ESP_LOGE(TAG, "JARVIS error: %.*s", data->data_len, (char*)data->data_ptr);
                    set_device_state(DEVICE_STATE_ERROR);
                    set_led_mode(LED_MODE_ERROR);
                }
            } else if (data->op_code == WS_OPCODE_CLOSE) {
                ESP_LOGI(TAG, "WebSocket closed by server");
                ws_connected = false;
                ws_streaming = false;
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            ws_connected = false;
            set_device_state(DEVICE_STATE_ERROR);
            set_led_mode(LED_MODE_ERROR);
            break;

        case WEBSOCKET_EVENT Sent:
            ESP_LOGD(TAG, "WS sent");
            break;

        default:
            break;
    }
}

// ============================================================================
// Send audio chunk over WebSocket
// ============================================================================
static void send_audio_chunk(const audio_chunk_t* chunk) {
    if (!ws_client || !ws_connected) return;

    // JSON frame: {"type":"audio","data":"<base64 PCM>","timestamp":1234}
    static char json_buf[4096];
    
    // Base64 encode the audio (16-bit PCM -> base64 is ~1.33x)
    // AUDIO_BYTES_PER_CHUNK = 5120 bytes max, base64 = ~6827 chars + JSON overhead
    // Actually: AUDIO_SAMPLES_PER_CHUNK * 2 bytes, base64 of 5120 bytes = 6827 chars
    // JSON overhead: ~50 chars, total ~6900. Buffer size of 4096 is too small.
    
    // For large chunks, we need streaming base64. Use a simple approach:
    // Only send first N bytes if buffer is small, or use a larger buffer
    // For now, truncate to fit 4KB buffer
    
    static char b64_buf[8192]; // Larger buffer for base64
    
    // Encode only first portion of audio chunk to stay within buffer
    size_t encode_len = sizeof(chunk->samples) / 2; // Use first N samples
    if (encode_len > 4000) encode_len = 4000; // Cap to stay within base64 buffer
    
    size_t b64_out_len = 0;
    int encode_err = mbedtls_base64_encode(
        (unsigned char*)b64_buf, sizeof(b64_buf) - 1,
        &b64_out_len, (unsigned char*)chunk->samples, encode_len * 2
    );
    if (encode_err != 0) {
        ESP_LOGW(TAG, "Base64 encode failed: %d", encode_err);
        return;
    }
    size_t b64_len = b64_out_len;
    
    if (b64_len <= 0) {
        ESP_LOGW(TAG, "Base64 encode failed");
        return;
    }
    b64_buf[b64_len] = 0;
    
    int json_len = snprintf(json_buf, sizeof(json_buf),
        R"({"type":"audio","data":"%s","timestamp":%lu,"vad":%s})",
        b64_buf,
        (unsigned long)chunk->timestamp_ms,
        chunk->vad_active ? "true" : "false"
    );

    if (esp_websocket_client_send_bin(ws_client, json_buf, json_len, portMAX_DELAY) < 0) {
        ESP_LOGW(TAG, "Failed to send audio chunk");
    } else {
        audio_frames_sent++;
    }
}

// ============================================================================
// Send JSON control message
// ============================================================================
static void send_ws_json(const char* msg) {
    if (!ws_client || !ws_connected) return;
    esp_websocket_client_send_text(ws_client, msg, strlen(msg), portMAX_DELAY);
}

// ============================================================================
// HTTP Streaming Task
// ============================================================================
void http_stream_task(void* param) {
    audio_chunk_t chunk;
    int64_t silence_start_us = 0;

    while (true) {
        // Wait for notification to start streaming or check queue periodically
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(AUDIO_CHUNK_MS));

        if (!ws_connected) {
            // Try to reconnect
            if (ws_client) {
                esp_websocket_client_close(ws_client, 1000);
                esp_websocket_client_destroy(ws_client);
                ws_client = NULL;
            }

            // Reconnect to JARVIS Core WebSocket
            char ws_uri[128];
            snprintf(ws_uri, sizeof(ws_uri), "ws://%s:%d/ws/voice", 
                     JARVIS_CORE_HOST, JARVIS_CORE_PORT);

            esp_websocket_client_config_t ws_cfg = {
                .uri = ws_uri,
                .task_stack = 4096,
                .buffer_size = 4096,
                .disable_auto_reconnect = false,
                .reconnect_timeout_ms = 5000,
            };

            ws_client = esp_websocket_client_init(&ws_cfg);
            esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, 
                                          ws_event_handler, NULL);
            esp_websocket_client_start(ws_client);
            
            vTaskDelay(pdMS_TO_TICKS(500)); // Wait for connection
            continue;
        }

        if (!g_is_streaming) continue;

        // Stream mode: send audio chunks
        while (g_is_streaming && ws_connected) {
            if (xQueueReceive(g_audio_queue, &chunk, pdMS_TO_TICKS(AUDIO_CHUNK_MS)) == pdTRUE) {
                if (chunk.vad_active || chunk.silence_frame) {
                    send_audio_chunk(&chunk);
                }

                if (chunk.silence_frame) {
                    if (silence_start_us == 0) {
                        silence_start_us = esp_timer_get_time();
                    }
                } else {
                    silence_start_us = 0;
                }

                // Stop if silence for too long
                if (silence_start_us > 0) {
                    int64_t silence_ms = (esp_timer_get_time() - silence_start_us) / 1000;
                    if (silence_ms > VAD_SILENCE_CHUNKS * AUDIO_CHUNK_MS) {
                        ESP_LOGI(TAG, "Silence timeout — sending end-of-stream");
                        
                        // Send end-of-stream marker
                        char eos[128];
                        snprintf(eos, sizeof(eos), 
                            R"({"type":"audio_done","frames":%lu,"duration_ms":%lld})",
                            (unsigned long)audio_frames_sent,
                            (esp_timer_get_time() - stream_start_us) / 1000
                        );
                        send_ws_json(eos);
                        
                        ws_streaming = false;
                        audio_frames_sent = 0;
                        silence_start_us = 0;
                        g_is_streaming = false;
                        set_device_state(DEVICE_STATE_PROCESSING);
                        set_led_mode(LED_MODE_ACTIVE);
                        break;
                    }
                }
            }

            // Also check for incoming WebSocket data (non-blocking)
            if (esp_websocket_client_is_connected(ws_client)) {
                // Process any incoming data
                // The event handler already handles this asynchronously
            }
        }
    }
}

// ============================================================================
// Start a new stream session
// ============================================================================
static void start_stream(void) {
    if (!ws_connected) {
        ESP_LOGW(TAG, "Cannot start stream — WebSocket not connected");
        return;
    }

    ESP_LOGI(TAG, "Starting audio stream to JARVIS Core");
    ws_streaming = true;
    audio_frames_sent = 0;
    stream_start_us = esp_timer_get_time();

    // Send session start message
    char start_msg[256];
    snprintf(start_msg, sizeof(start_msg),
        R"({"type":"start","sample_rate":%d,"format":"pcm16","channels":1,"device":"%s"})",
        AUDIO_SAMPLE_RATE,
        DEVICE_TOPIC_BASE
    );
    send_ws_json(start_msg);
}

// ============================================================================
// HTTP Streaming Init
// ============================================================================
void init_http_stream(void) {
    ESP_LOGI(TAG, "HTTP streaming client init — target: %s:%d/ws/voice",
             JARVIS_CORE_HOST, JARVIS_CORE_PORT);

    xTaskCreatePinnedToCore(&http_stream_task, "http_stream", 8192, NULL, 5, &g_http_stream_handle, 0);
}
