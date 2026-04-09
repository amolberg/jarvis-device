# JARVIS Device — DIY Room Node Firmware & Hardware

Build your own JARVIS hardware nodes. Each device is a small ESP32 board with microphone and LED ring that sits on your desk and lets you talk to JARVIS hands-free.

---

## 🧰 Bill of Materials (~$17 per device)

| Component | Model | Price | Notes |
|-----------|-------|-------|-------|
| Microcontroller | ESP32-S3-DevKitC-1 | ~$10 | Dual-core, WiFi, USB-C |
| Microphone | INMP441 | ~$3 | I2S digital mic, omnidirectional |
| LED Ring | WS2812B (24 LED) | ~$2 | RGB, individually addressable |
| Temp/Humidity | BME280 | ~$2 | I2C, optional but cool |
| Capacitor | 100µF 16V | ~$0.10 | Stabilizes power |
| **Total** | | **~$17** | |

### Optional Upgrades

| Component | Price | Notes |
|-----------|-------|-------|
| ReSpeaker 2-Mic Pi HAT | ~$10 | All-in-one, no soldering |
| 3W Class D Amplifier | ~$2 | For external speaker |
| 4Ω 3W Speaker | ~$2 | Small speaker for audio out |
| 3D Printed Case | ~$0 | PLA plastic, files included |

---

## 🔧 Quick Start

### 1. Assemble the Hardware

Wire connections:

```
ESP32-S3                  INMP441 (I2S Microphone)
─────────────            ──────────
GPIO 4  ──── SD_DATA     VDD  ──── 3.3V
GPIO 5  ──── SCK         GND  ──── GND
GPIO 6  ──── WS          SD   ──── GPIO 4
3.3V   ──── VDD          SCK  ──── GPIO 5
GND    ──── GND           WS   ──── GPIO 6
                          L/R  ──── GND

ESP32-S3                  WS2812B (LED Ring)
─────────────            ──────────
GPIO 48 ──── DATA_IN     5V   ──── USB 5V
GND     ──── GND          GND  ──── GND

ESP32-S3                  BME280 (Temp/Humidity)
─────────────            ──────────
GPIO 1  ──── SDA         VIN  ──── 3.3V
GPIO 2  ──── SCL         GND  ──── GND
3.3V   ──── VCC          SDA  ──── GPIO 1
GND    ──── GND           SCL  ──── GPIO 2
```

### 2. Flash the Firmware

```bash
# Install PlatformIO
pip install platformio

# Clone firmware
cd firmware
pio run --environment esp32dev --target upload
```

### 3. Configure

Connect to the device's WiFi (AP mode: `JARVIS-Device-XXXX`) or configure via USB serial:

```bash
# Serial configuration
pio device monitor --baud 115200
```

Set:
- WiFi SSID + password
- JARVIS Core server URL: `http://jarvis.local:8080`
- Device name: "Living Room", "Kitchen", etc.

### 4. Test

```bash
# Say wake word, then speak
# LED shows state: blue=listening, purple=processing, green=speaking
```

---

## 📁 Project Structure

```
jarvis-device/
├── firmware/
│   ├── src/
│   │   ├── main.cpp         # Entry point, loop
│   │   ├── audio.cpp        # I2S mic capture
│   │   ├── led.cpp          # WS2812B animation
│   │   ├── wifi.cpp         # WiFi + MQTT
│   │   └── jarvis.cpp       # JARVIS Core API client
│   ├── include/             # Header files
│   └── platformio.ini       # Build config
├── hardware/
│   ├── case/                # 3D printable STL files
│   └── pcb/                 # KiCad PCB designs (optional)
├── bom/                     # Bill of materials with supplier links
└── README.md
```

---

## 💡 Features

- **Wake Word** — Listens for "Hey JARVIS" (Porcupine on-device)
- **Voice Streaming** — Sends audio to JARVIS Core via WebSocket
- **Smart Home** — MQTT client for Home Assistant
- **LED Feedback** — Visual state: idle, listening, processing, speaking
- **OTA Updates** — Update firmware over WiFi, no USB needed
- **IR/RF Support** — Control TVs, AC, etc. (with additional hardware)

---

## 🔌 LED States

| Color | State |
|-------|-------|
| 💤 Dim white | Idle, listening |
| 🔵 Blue pulse | Listening for command |
| 🟣 Purple | Processing with JARVIS |
| 🟢 Green | JARVIS speaking |
| 🔴 Red flash | Error |
| 🌈 Rainbow | Startup |

---

## 🔒 Privacy

All audio processing happens on-device (wake word) and on your JARVIS server. Nothing is sent to the cloud. Your voice never leaves your home network.

---

## 📜 License

MIT — Build it, break it, improve it.
