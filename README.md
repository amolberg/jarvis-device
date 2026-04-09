# JARVIS Device — DIY Room Node

Build your own $18 voice AI node for every room. Each device has a microphone, LED ring, and connects wirelessly to your JARVIS Core server — no cloud required.

**See it in action:** Open the JARVIS Dashboard → Voice tab. Say "Hey JARVIS" and talk.

---

## 🛒 Bill of Materials (~$18 per device)

| Component | Model | Price | Notes |
|-----------|-------|-------|-------|
| Microcontroller | ESP32-S3-DevKitC-1 N8R8 | ~$9 | Dual-core, WiFi, USB-C |
| Microphone | INMP441 | ~$2 | I2S digital mic, no soldering with ReSpeaker HAT |
| LED Ring | WS2812B 24-LED | ~$2 | RGB, individually addressable |
| Capacitor | 100µF 16V | ~$0.10 | Stabilizes LED power |
| Capacitors | 100nF ceramic ×3 | ~$0.15 | Power decoupling |
| Wires | DuPont kit | ~$3 | Breadboard wiring |
| **Total** | | **~$18** | |

**Full BOM with real supplier links:** [`bom/bom.csv`](bom/bom.csv)

### Build Options

| Option | Cost | Notes |
|--------|------|-------|
| **Core** | ~$18 | ESP32-S3 + mic + LEDs only |
| **+ Speaker** | ~$23 | Add PAM8403 amp + 4Ω speaker |
| **+ Climate** | ~$25 | Above + BME280 temp/humidity sensor |
| **No-solder** | ~$22 | ESP32-S3 + ReSpeaker 2-Mic HAT |
| **5-pack PCB** | ~$28/unit | Custom PCB, JLCPCB fab |

---

## ⚡ Quick Start

### 1. Assemble (30 min)

**Wiring diagram:**
```
ESP32-S3              INMP441
────────────          ──────
GPIO4  ──── SD_DATA   VDD  ──── 3.3V
GPIO5  ──── SCK       GND  ──── GND
GPIO6  ──── WS        SD   ──── GPIO4
3.3V   ──── VDD      SCK  ──── GPIO5
GND    ──── GND       WS   ──── GPIO6
                       L/R  ──── GND

ESP32-S3              WS2812B Ring
────────────          ────────────
GPIO48 ──── DATA_IN   5V   ──── USB 5V
GND     ──── GND      GND  ──── GND
```

See [`docs/ASSEMBLY.md`](docs/ASSEMBLY.md) for detailed step-by-step instructions, or [`hardware/wiring-diagram.svg`](hardware/wiring-diagram.svg) for a visual reference.

### 2. Flash Firmware

```bash
# Install ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
./install.sh esp32s3
source ~/esp/esp-idf/export.sh

# Build and flash
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Or use [PlatformIO](https://platformio.org):
```bash
pip install platformio
pio run --environment esp32dev --target upload
```

### 3. Configure

Connect to serial (115200 baud):
```
> wifi_connect YourSSID YourPassword
> config
JARVIS Core URL: http://10.0.0.74:8080
Device Name: Living Room
```

### 4. Test

Open the JARVIS Dashboard → Voice tab. The LED ring shows the device state:

| LED | Meaning |
|-----|---------|
| 💤 Blue pulse | Idle, listening |
| 🔵 Bright blue | Detected speech |
| 🟣 Purple | Processing with LLM |
| 🟢 Green | JARVIS speaking |
| 🔴 Red | Error |

---

## 📁 Project Structure

```
jarvis-device/
├── firmware/              # ESP32-S3 C++ firmware (ESP-IDF)
│   ├── src/
│   │   ├── main.cpp         # Entry point, state machine
│   │   ├── audio.cpp        # I2S mic capture, RMS VAD
│   │   ├── led.cpp          # WS2812B LED animations
│   │   ├── wifi.cpp         # WiFi manager, NVS storage
│   │   ├── mqtt.cpp         # MQTT client, HA auto-discovery
│   │   └── http_stream.cpp  # WebSocket audio to JARVIS Core
│   ├── include/
│   │   ├── config.h         # Network, audio, LED pins
│   │   └── states.h         # State machine types
│   ├── platformio.ini       # PlatformIO build
│   └── CMakeLists.txt       # ESP-IDF build
├── hardware/               # Hardware designs
│   ├── case/
│   │   └── jarvis-case.scad  # Parametric OpenSCAD case
│   ├── pcb/                 # KiCad PCB (future)
│   └── wiring-diagram.svg    # Full wiring reference
├── bom/
│   └── bom.csv              # Parts list with supplier links
├── docs/
│   └── ASSEMBLY.md          # Step-by-step build guide
├── .github/
│   └── ISSUE_TEMPLATE/      # Bug reports, feature requests
├── CONTRIBUTING.md
└── README.md
```

---

## 🔧 Features

| Feature | Status | Notes |
|---------|--------|-------|
| **Wake Word** | ✅ | VAD-based always-listening (Porcupine optional) |
| **Voice Streaming** | ✅ | WebSocket audio to JARVIS Core `/ws/voice` |
| **Smart Home** | ✅ | MQTT client for Home Assistant |
| **LED Feedback** | ✅ | 7 animation modes per state |
| **OTA Updates** | ✅ | HTTP OTA from JARVIS Core |
| **IR/RF Support** | 🔜 | Coming soon |
| **Wake Word Engine** | 🔜 | Porcupine on-device |
| **Streaming STT** | 🔜 | Real-time transcription |

---

## 🔌 LED States & Animations

| State | Animation |
|-------|----------|
| Idle | Slow blue pulse |
| Listening | Breathing cyan |
| VAD triggered | White sweep |
| Processing | Spinning blue dot |
| Speaking | Spinning green dot |
| Alert | Orange flash |
| Error | Solid red |
| Startup | Rainbow spin |

---

## 🔒 Privacy

All audio processing happens **on-device** (VAD) and on **your JARVIS server**. Nothing is sent to the cloud. Your voice never leaves your home network.

---

## 🐛 Troubleshooting

**No serial output on boot:**
- Try a different USB cable (data-capable, not charge-only)
- Hold BOOT, press RESET, release BOOT → try flashing again

**WiFi won't connect:**
- ESP32-S3 only supports 2.4GHz (not 5GHz)
- Double-check SSID/password (case-sensitive)
- `idf.py erase-flash` to wipe and start fresh

**LED ring wrong colors / flickering:**
- Add 100µF capacitor across 5V/GND near the ring
- Check data wire connection (GPIO48 → DIN)
- Reduce brightness in `config.h`

**INMP441 no audio:**
- L/R pin MUST be connected to GND
- Verify 3.3V power to the mic
- Inspect I2S solder joints under magnification

---

## 🤝 Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for setup guide, code style, and PR process.

---

## 📜 License

MIT — Zebratic 2026 — Build it, break it, improve it.
