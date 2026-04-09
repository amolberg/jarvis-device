#ifndef JARVIS_DEVICE_CONFIG_H
#define JARVIS_DEVICE_CONFIG_H

// ============================================================================
// JARVIS Device — Configuration
// Auto-generated / editable before flash. Runtime overrides via MQTT.
// ============================================================================

// --- Network ---
#ifndef WIFI_SSID
#define WIFI_SSID         "YourWiFiSSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD     "YourWiFiPassword"
#endif

// --- JARVIS Core ---
// Where to send audio and receive commands
#ifndef JARVIS_CORE_HOST
#define JARVIS_CORE_HOST  "10.0.0.74"   // JARVIS Core IP
#endif

#ifndef JARVIS_CORE_PORT
#define JARVIS_CORE_PORT  8080
#endif

// --- MQTT (Home Assistant) ---
#ifndef MQTT_SERVER
#define MQTT_SERVER       "10.0.0.7"    // Home Assistant IP
#endif

#ifndef MQTT_PORT
#define MQTT_PORT         1883
#endif

#ifndef MQTT_USERNAME
#define MQTT_USERNAME     ""
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD     ""
#endif

// Base topic for this device — set via MQTT config or compile-time
#ifndef DEVICE_TOPIC_BASE
#define DEVICE_TOPIC_BASE  "home/jarvis-device/living-room"
#endif

// --- Audio ---
#define AUDIO_SAMPLE_RATE     16000
#define AUDIO_BIT_DEPTH       16
#define AUDIO_CHANNELS        1        // Mono
#define AUDIO_BYTES_PER_SAMPLE 2
#define AUDIO_CHUNK_MS        160      // ~2560 bytes at 16kHz/16bit
#define AUDIO_SAMPLES_PER_CHUNK (AUDIO_SAMPLE_RATE * AUDIO_CHUNK_MS / 1000)

// --- I2S Microphone (INMP441) ---
#define I2S_WS_PIN    6
#define I2S_SCK_PIN   5
#define I2S_SD_PIN    4
#define I2S_PORT      I2S_NUM_1

// --- LED Ring (WS2812B) ---
#define LED_PIN        48
#define LED_COUNT      24

// --- I2C (BME280) ---
#define I2C_SDA_PIN    1
#define I2C_SCL_PIN    2
#define I2C_FREQ       400000

// --- Voice Activity Detection ---
// Audio level threshold for "speech detected" (0–32767 for 16-bit PCM)
#define VAD_THRESHOLD       500
// How many consecutive chunks above threshold to trigger wake
#define VAD_MIN_CHUNKS      3
// Silence chunks before stopping stream
#define VAD_SILENCE_CHUNKS  8

// --- Wake Word (simple VAD — for advanced, use MicroWakeNet) ---
// The device uses audio level detection. For real wake-word,
// flash Porcupine .ppn file to SPIFFS and use picovoice-esp32.
// This firmware uses VAD-based "always listening" + JARVIS activation.
#define USE_SIMPLE_VAD      1

// --- OTA ---
#define OTA_ENABLED          1
#define OTA_URL              "http://" JARVIS_CORE_HOST "/ota/jarvis-device.bin"
#define OTA_CHECK_INTERVAL_H 6

// --- Deep Sleep ---
#define DEEP_SLEEP_TIMEOUT_S 300  // 5 min inactivity → light sleep

#endif // JARVIS_DEVICE_CONFIG_H
