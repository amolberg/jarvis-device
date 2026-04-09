#ifndef JARVIS_DEVICE_STATES_H
#define JARVIS_DEVICE_STATES_H

#include <stdint.h>

// ============================================================================
// JARVIS Device — State Machine
// ============================================================================

typedef enum {
    DEVICE_STATE_INIT = 0,      // Booting up
    DEVICE_STATE_CONNECTING,    // WiFi / MQTT connecting
    DEVICE_STATE_IDLE,          // Listening for voice (LED ring pulses slowly)
    DEVICE_STATE_AUDIO_READY,   // Audio buffers ready
    DEVICE_STATE_STREAMING,     // Actively streaming audio to JARVIS
    DEVICE_STATE_PROCESSING,    // JARVIS is processing / TTS playing
    DEVICE_STATE_SPEAKING,      // TTS audio playing through speaker
    DEVICE_STATE_ERROR,         // Error state (LED turns red)
    DEVICE_STATE_SLEEPING,      // Low-power sleep mode
} device_state_t;

typedef enum {
    LED_MODE_OFF      = 0,
    LED_MODE_PULSE    = 1,   // Slow blue pulse — idle
    LED_MODE_LISTEN   = 2,   // Breathing cyan — listening for speech
    LED_MODE_ACTIVE   = 3,   // Spinning blue — JARVIS processing
    LED_MODE_SPEAKING = 4,   // Spinning green — JARVIS speaking
    LED_MODE_ALERT    = 5,   // Orange flash — notification
    LED_MODE_ERROR    = 6,   // Solid red — error
    LED_MODE_WAKE     = 7,   // White sweep — wake word detected
} led_mode_t;

// MQTT command types (JSON payload)
typedef enum {
    MQTT_CMD_NONE = 0,
    MQTT_CMD_OTA_CHECK,
    MQTT_CMD_OTA_START,
    MQTT_CMD_SET_LED,
    MQTT_CMD_SET_VOLUME,
    MQTT_CMD_SLEEP,
    MQTT_CMD_WAKE,
    MQTT_CMD_STATUS,
    MQTT_CMD_RESET,
} mqtt_cmd_t;

// Connection status flags
typedef struct {
    uint8_t wifi_connected : 1;
    uint8_t mqtt_connected : 1;
    uint8_t jarvis_reachable : 1;
    uint8_t audio_ok : 1;
    uint8_t led_ok : 1;
} connection_status_t;

// Audio buffer for streaming
typedef struct {
    int16_t samples[AUDIO_SAMPLES_PER_CHUNK];
    uint32_t timestamp_ms;
    uint8_t vad_active : 1;
    uint8_t silence_frame : 1;
} audio_chunk_t;

// Device configuration (persisted to NVS)
typedef struct {
    char wifi_ssid[64];
    char jarvis_host[64];
    uint16_t jarvis_port;
    char device_name[32];
    char mqtt_topic_base[64];
    uint8_t led_brightness;    // 0–255
    uint8_t vad_threshold;     // 0–255 (normalized)
    uint8_t auto_sleep_min;   // minutes
    uint32_t fw_version;
} device_config_t;

#endif // JARVIS_DEVICE_STATES_H
