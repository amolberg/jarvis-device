# JARVIS Device Firmware

ESP32-S3 firmware for JARVIS room nodes. Handles microphone capture, voice activity detection, LED ring animations, MQTT connectivity to Home Assistant, and WebSocket audio streaming to JARVIS Core.

## Features

- **I2S Microphone (INMP441)** — 16kHz mono PCM audio capture
- **Voice Activity Detection** — RMS-based VAD to detect when user speaks
- **LED Ring (WS2812B)** — Animated status feedback (pulse, breathe, spin)
- **MQTT** — Connect to Home Assistant MQTT broker for commands and status
- **WebSocket Streaming** — Bidirectional audio to JARVIS Core `/ws/voice`
- **OTA Updates** — Update firmware over HTTP from JARVIS Core
- **Deep Sleep** — Low-power mode after inactivity

## Hardware

- **ESP32-S3-DevKitC-1** — Dual-core, WiFi, USB-C
- **INMP441** — I2S digital microphone
- **WS2812B** — 24-LED RGB ring (optional)
- **BME280** — Temperature/humidity sensor (optional)

## Quick Start

### Prerequisites

```bash
# Install ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
./install.sh esp32s3
source ~/esp/esp-idf/export.sh

# Or use ESP-IDF 5.x via pip
pip install espressif-esp-idf
```

### Configure

Edit `include/config.h` to set your network and JARVIS Core:

```c
#define WIFI_SSID         "YourWiFiSSID"
#define WIFI_PASSWORD     "YourWiFiPassword"
#define JARVIS_CORE_HOST  "10.0.0.74"    // Your JARVIS Core IP
#define MQTT_SERVER       "10.0.0.7"     // Home Assistant IP
```

### Build

```bash
cd firmware
idf.py set-target esp32s3
idf.py menuconfig   # Optional: configure partition table, flash size
idf.py build
```

### Flash

```bash
# Connect ESP32-S3 via USB and flash
idf.py -p /dev/ttyUSB0 flash monitor

# Or flash only
idf.py -p /dev/ttyUSB0 flash

# For OTA updates (after first flash)
idf.py app
# The .bin file is at build/jarvis-device-firmware.bin
```

### Monitor

```bash
idf.py -p /dev/ttyUSB0 monitor
```

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    ESP32-S3 Firmware                          │
│                                                               │
│  ┌─────────────┐   ┌──────────┐   ┌────────────────────┐   │
│  │ I2S Mic     │──▶│ VAD      │──▶│ Audio Queue        │   │
│  │ (INMP441)   │   │ (RMS)    │   │                    │   │
│  └─────────────┘   └──────────┘   └─────────┬──────────┘   │
│                                              │               │
│  ┌─────────────┐   ┌──────────┐   ┌─────────▼──────────┐   │
│  │ MQTT Client │◀──│ Cmd Queue│◀──│ State Machine      │   │
│  │ (HA broker)│   │          │   │                    │   │
│  └─────────────┘   └──────────┘   └─────────┬──────────┘   │
│                                              │               │
│  ┌─────────────┐   ┌──────────┐   ┌─────────▼──────────┐   │
│  │ WS2812B     │◀──│ LED Anim │◀──│ Status Updates     │   │
│  │ (24 LEDs)   │   │ (30fps)  │   │                    │   │
│  └─────────────┘   └──────────┘   └────────────────────┘   │
│                                              │               │
│  ┌──────────────────────────────────────────▼────────────┐  │
│  │          WebSocket Client → JARVIS Core /ws/voice    │  │
│  │   Sends: JSON {"type":"audio","data":"<b64 PCM>"}    │  │
│  │   Receives: transcript, llm_done, audio chunks        │  │
│  └──────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

## State Machine

```
[INIT] → [CONNECTING] → [IDLE] ──────────────────┐
                                  │                │
                     ┌────────────┴───┐            │
                     ▼                ▼            │
              [AUDIO_READY]    [STREAMING] ◀───────┤
                                                   │
                                    ┌──────────────┴───────┐
                                    ▼                      ▼
                              [PROCESSING]           [SPEAKING]
                                    │                      │
                                    └──────────┬───────────┘
                                               ▼
                                            [IDLE]
```

## LED Animations

| State | Animation |
|-------|----------|
| Idle | Slow blue pulse |
| Listening | Breathing cyan |
| Processing | Spinning blue dot |
| Speaking | Spinning green dot |
| Alert | Orange flash |
| Error | Solid red |
| Wake | White sweep |

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `home/jarvis-device/<id>/cmd` | In | Command (`{"cmd":"sleep\|wake\|reset"}`) |
| `home/jarvis-device/<id>/state` | Out | Device state JSON |
| `home/jarvis-device/<id>/avail` | Out | `online` / `offline` |

## WebSocket Protocol

Connect to `ws://<JARVIS_CORE_HOST>:8080/ws/voice`

**Send:**
```json
{"type":"start","sample_rate":16000,"format":"pcm16","channels":1}
{"type":"audio","data":"<base64 PCM>","timestamp":1234,"vad":true}
{"type":"audio_done","frames":42,"duration_ms":1680}
```

**Receive:**
```json
{"type":"transcript","text":"turn on the lights"}
{"type":"llm_chunk","delta":"turn"}
{"type":"llm_done","text":"Turning on the lights."}
{"type":"audio","data":"<base64 MP3>","chunk":true}
{"type":"audio_done"}
{"type":"error","message":"..."}
```

## OTA Updates

Place firmware binary at `http://<JARVIS_CORE_HOST>/ota/jarvis-device.bin`. Device checks for updates every 6 hours or when triggered via MQTT command `{"cmd":"ota"}`.

## Troubleshooting

**Audio clicks/pops:** Normal — I2S timing varies. Increase VAD threshold if needed.

**WiFi won't connect:** Check SSID/password in `config.h`. Verify NVS hasn't cached wrong credentials: `idf.py erase-flash`.

**MQTT disconnects:** Home Assistant MQTT add-on must be enabled. Check `homeassistant` status in HA.

**WebSocket fails:** JARVIS Core must be running on port 8080. Check `curl http://<JARVIS_CORE_HOST>:8080/api/health`.

## License

MIT — amolberg
