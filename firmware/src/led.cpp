/**
 * JARVIS Device — LED Ring Driver (WS2812B)
 * 
 * Animated status LEDs for hands-free feedback:
 * - Idle: slow blue pulse
 * - Listening: breathing cyan
 * - Processing: spinning blue
 * - Speaking: spinning green
 * - Error: solid red
 * - Wake: white sweep
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <driver/rmt.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

#include "config.h"
#include "states.h"

static const char* TAG = "led";

extern SemaphoreHandle_t g_state_mutex;
extern led_mode_t g_led_mode;

// RMT TX channel for WS2812B
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

// Animation state
static uint32_t g_led_step = 0;
static uint32_t g_led_color_r = 0;
static uint32_t g_led_color_g = 0;
static uint32_t g_led_color_b = 0;

// LED strip pixel buffer (24 LEDs)
static uint32_t leds[LED_COUNT];

// ============================================================================
// Color helpers
// ============================================================================
static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static inline uint32_t color_brightness(uint32_t color, float brightness) {
    uint8_t r = ((color >> 16) & 0xFF) * brightness;
    uint8_t g = ((color >> 8) & 0xFF) * brightness;
    uint8_t b = (color & 0xFF) * brightness;
    return rgb(r, g, b);
}

// ============================================================================
// RMT LED driver
// ============================================================================
static esp_err_t ws2812_init(void) {
    ESP_LOGI(TAG, "Initializing WS2812B LED ring on GPIO %d, %d LEDs", LED_PIN, LED_COUNT);

    // RMT TX channel
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = (gpio_num_t)LED_PIN,
        .clk_src = RMT_CLK_SRC_APB,
        .resolution_hz = 10'000'000,  // 10MHz = 100ns per tick
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags = {
            .invert_out = false,
            .with_dma = false,
        },
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &led_chan));

    // Simple LED encoder (WS2812B timing)
    rmt_led_encoder_config_t enc_cfg = {
        .resolution_hz = 10'000'000,
    };
    ESP_ERROR_CHECK(rmt_new_led_encoder(&enc_cfg, &led_encoder));

    ESP_ERROR_CHECK(rmt_enable(led_chan));
    ESP_LOGI(TAG, "WS2812B initialized");

    // Clear all LEDs
    memset(leds, 0, sizeof(leds));
    return ESP_OK;
}

// Send LED data to strip
static void led_refresh(void) {
    if (!led_chan || !led_encoder) return;

    rmt_transmit_config_t tx_conf = {
        .loop_count = 0,
        .flags = { .eot_level = 0, .queue_non_empty = false },
    };

    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, leds, sizeof(leds), &tx_conf));
}

// ============================================================================
// LED Animation Modes
// ============================================================================

// Fill entire strip with solid color
static void led_fill(uint32_t color) {
    for (int i = 0; i < LED_COUNT; i++) leds[i] = color;
}

// Breathing animation (slow fade in/out)
static uint32_t led_breathing(uint32_t base_color, uint32_t step, uint32_t period) {
    // Sine wave breathing
    float t = (step % period) / (float)period;
    float brightness = (sin(t * 2 * M_PI) + 1.0f) / 2.0f; // 0 to 1
    brightness = 0.1f + brightness * 0.9f; // Min 10% brightness
    return color_brightness(base_color, brightness);
}

// Spinning dot
static void led_spinning_dot(uint32_t color, uint32_t step, int speed) {
    // Clear all
    for (int i = 0; i < LED_COUNT; i++) leds[i] = 0;
    
    // Position of dot
    int pos = (step / speed) % LED_COUNT;
    int trail = LED_COUNT / 8; // Trail length
    
    for (int t = 0; t < trail; t++) {
        int idx = (pos - t + LED_COUNT) % LED_COUNT;
        float b = 1.0f - (t / (float)trail) * 0.7f;
        leds[idx] = color_brightness(color, b);
    }
}

// Pulse animation
static void led_pulse(uint32_t color, uint32_t step) {
    for (int i = 0; i < LED_COUNT; i++) leds[i] = color;
    float brightness = 0.3f + 0.7f * ((step % 100) / 100.0f);
    for (int i = 0; i < LED_COUNT; i++) {
        leds[i] = color_brightness(leds[i], brightness);
    }
}

// ============================================================================
// LED Task — runs animation loop at ~30fps
// ============================================================================
void led_task(void* param) {
    ESP_LOGI(TAG, "LED task started");

    ws2812_init();
    
    uint32_t step = 0;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        led_mode_t mode;
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        mode = g_led_mode;
        xSemaphoreGive(g_state_mutex);

        switch (mode) {
            case LED_MODE_OFF:
                led_fill(0);
                break;

            case LED_MODE_PULSE:
                // Slow blue pulse (idle) — 60 step cycle
                led_fill(led_breathing(rgb(0, 0, 180), step, 120));
                break;

            case LED_MODE_LISTEN:
                // Breathing cyan — user speaking
                led_fill(led_breathing(rgb(0, 200, 255), step, 60));
                break;

            case LED_MODE_ACTIVE:
                // Spinning blue dot — JARVIS processing
                led_spinning_dot(rgb(50, 100, 255), step, 4);
                break;

            case LED_MODE_SPEAKING:
                // Spinning green dot — JARVIS speaking
                led_spinning_dot(rgb(50, 255, 100), step, 6);
                break;

            case LED_MODE_ALERT:
                // Orange flash
                led_fill(step % 20 < 10 ? rgb(255, 140, 0) : rgb(0, 0, 0));
                break;

            case LED_MODE_ERROR:
                // Solid red
                led_fill(rgb(255, 0, 0));
                break;

            case LED_MODE_WAKE:
                // White sweep
                led_spinning_dot(rgb(255, 255, 255), step, 3);
                break;

            default:
                led_fill(0);
                break;
        }

        led_refresh();
        step++;

        // 30fps animation
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(33));
    }
}

// ============================================================================
// LED initialization (called from main.cpp)
// ============================================================================
void init_led(void) {
    // LED task handles initialization
    ESP_LOGI(TAG, "LED init scheduled (handled in led_task)");
}
