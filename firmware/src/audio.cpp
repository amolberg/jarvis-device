/**
 * JARVIS Device — Audio Capture (I2S + VAD)
 * 
 * Captures PCM audio from INMP441 I2S microphone.
 * Runs simple voice activity detection (VAD) to detect when user speaks.
 * Feeds audio chunks to the audio queue for HTTP streaming.
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_dsp.h>

#include "config.h"
#include "states.h"

// ============================================================================
// Globals (from main.cpp)
// ============================================================================
extern SemaphoreHandle_t g_state_mutex;
extern QueueHandle_t g_audio_queue;
extern bool g_is_streaming;
extern uint8_t g_vad_threshold;

// ============================================================================
// I2S Configuration (INMP441)
// ============================================================================
static const char* TAG = "audio";

    // Note: i2s_config defined inline in init_audio() — this is a stub reference only

static i2s_chan_handle_t i2s_handle = NULL;
static RingbufHandle_t rb = NULL;

// VAD state
static int g_silence_chunks = 0;
static int g_speech_chunks = 0;
static bool g_vad_triggered = false;

// ============================================================================
// Audio task
// ============================================================================
void audio_task(void* param) {
    ESP_LOGI(TAG, "Audio task started — sample rate: %d Hz, chunk: %d ms",
             AUDIO_SAMPLE_RATE, AUDIO_CHUNK_MS);

    // Init I2S
    i2s_channel_info_t info;
    ESP_ERROR_CHECK(i2s_channel_get_info(I2S_NUM_1, &info));

    if (info.role != I2S_ROLE_ADC_DAC) {
        // Channel not initialized yet
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(I2S_NUM_1, &i2s_config));
        ESP_ERROR_CHECK(i2s_channel_enable(I2S_NUM_1));
        
        // Get the ringbuffer
        ESP_ERROR_CHECK(i2s_channel_read(I2S_NUM_1, NULL, 0, NULL, 100)); // drain
    }

    uint8_t* read_buffer = (uint8_t*)malloc(AUDIO_BYTES_PER_CHUNK * 2);
    if (!read_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }

    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        // Read audio from I2S
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(
            I2S_NUM_1,
            read_buffer,
            AUDIO_BYTES_PER_CHUNK,
            &bytes_read,
            pdMS_TO_TICKS(AUDIO_CHUNK_MS + 10)
        );

        if (err == ESP_OK && bytes_read > 0) {
            int samples_read = bytes_read / AUDIO_BYTES_PER_SAMPLE;
            
            // Simple VAD: compute RMS audio level
            int32_t sum = 0;
            int16_t* samples = (int16_t*)read_buffer;
            for (int i = 0; i < samples_read; i++) {
                sum += abs(samples[i]);
            }
            int32_t avg = sum / samples_read;

            bool vad_active = avg > VAD_THRESHOLD;

            // Build audio chunk
            audio_chunk_t chunk;
            chunk.timestamp_ms = esp_log_timestamp();
            chunk.vad_active = vad_active;
            chunk.silence_frame = !vad_active;

            if (vad_active) {
                g_speech_chunks++;
                g_silence_chunks = 0;

                // Copy samples into chunk (cap at max)
                int to_copy = samples_read > AUDIO_SAMPLES_PER_CHUNK 
                    ? AUDIO_SAMPLES_PER_CHUNK 
                    : samples_read;
                memcpy(chunk.samples, read_buffer, to_copy * AUDIO_BYTES_PER_SAMPLE);
            } else {
                g_silence_chunks++;
                g_speech_chunks = 0;
            }

            // VAD logic: trigger on N consecutive speech chunks
            if (!g_vad_triggered && g_speech_chunks >= VAD_MIN_CHUNKS) {
                g_vad_triggered = true;
                ESP_LOGI(TAG, "VAD: Speaking started");
            }

            // Send chunk to queue if streaming is active
            if (g_is_streaming) {
                if (xQueueSend(g_audio_queue, &chunk, 0) != pdTRUE) {
                    // Queue full — drop chunk
                    ESP_LOGW(TAG, "Audio queue full, dropping chunk");
                }
            }

            // Stop VAD trigger after silence
            if (g_vad_triggered && g_silence_chunks >= VAD_SILENCE_CHUNKS) {
                g_vad_triggered = false;
                g_speech_chunks = 0;
                ESP_LOGI(TAG, "VAD: Speaking stopped");
            }

            ESP_LOGD(TAG, "Audio: rms=%ld vad=%d stream=%d", avg, vad_active, g_is_streaming);
        } else if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(err));
        }

        // Feed watchdog
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(AUDIO_CHUNK_MS));
    }

    free(read_buffer);
    vTaskDelete(NULL);
}

// ============================================================================
// Audio initialization
// ============================================================================
void init_audio(void) {
    ESP_LOGI(TAG, "Initializing I2S audio (INMP441 microphone)");
    
    // Install I2S driver
    i2s_chan_handle_t tx_handle;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_handle, NULL, &i2s_handle));
    
    // Configure I2S receiver (RX)
    i2s_config_t i2s_cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_RX | I2S_MODE_TX | I2S_MODE_PDM),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_desc_num = 8,
        .dma_frame_num = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };

    // Note: For INMP441 we use I2S_STD mode, not PDM
    // The config above is for PDM mics. For I2S mics like INMP441:
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_BITS_PER_SAMPLE_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = GPIO_NUM_5,
            .ws  = GPIO_NUM_6,
            .dout = GPIO_NUM_NC,
            .din  = GPIO_NUM_4,
        },
    };

    // Initialize I2S channel as RX (receive only)
    i2s_std_config_t rx_cfg = {
        .clk_cfg = {
            .sample_rate_hz = AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_APLL,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT | I2S_STD_SLOT_RIGHT,
            .ws_width = 16,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .clk = GPIO_NUM_5,
            .ws  = GPIO_NUM_6,
            .dout = GPIO_NUM_NC,
            .din  = GPIO_NUM_4,
            .invert_flags = {
                .clk_invert = false,
                .ws_invert = false,
            },
        },
    };

    i2s_chan_handle_t rx_handle;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_handle, NULL, &rx_handle));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    i2s_handle = rx_handle;

    ESP_LOGI(TAG, "I2S audio initialized — GPIO: WS=%d SCK=%d SD=%d", 
             I2S_WS_PIN, I2S_SCK_PIN, I2S_SD_PIN);
}
