# JARVIS Device — Flashing Instructions

This guide covers how to flash the JARVIS device firmware onto your ESP32-S3 DevKitC-1 board. Choose your method below.

---

## Method 1: PlatformIO (Recommended)

PlatformIO is the easiest way to build and flash the firmware. It handles all ESP-IDF dependencies automatically.

### Install PlatformIO

**VS Code:**
1. Install [VS Code](https://code.visualstudio.com)
2. Install the **PlatformIO IDE** extension (by PlatformIO)
3. Restart VS Code — PlatformIO will install itself automatically

**CLI only:**
```bash
pip install platformio
```

### Configure WiFi & JARVIS Core

Before flashing, edit `firmware/include/config.h`:

```c
#define WIFI_SSID         "YourWiFiSSID"
#define WIFI_PASSWORD     "YourWiFiPassword"
#define JARVIS_CORE_HOST  "10.0.0.74"   // Your JARVIS Core IP
#define MQTT_SERVER       "10.0.0.7"    // Your Home Assistant IP
```

Set a unique device name in the topic base:
```c
#define DEVICE_TOPIC_BASE  "home/jarvis-device/living-room"
```

### Flash via PlatformIO (VS Code)

1. Open the `firmware/` directory in VS Code: `File → Open Folder → firmware`
2. Connect your ESP32-S3 via USB-C
3. Open the **PIO Terminal** (icon in bottom-left toolbar, or `View → Terminal`)
4. Set target and flash:

```bash
pio run --environment esp32dev --target upload
```

5. To also see serial output:
```bash
pio device monitor
```

6. Or flash + monitor in one command:
```bash
pio run --environment esp32dev --target upload --target monitor
```

### Flash via PlatformIO (CLI)

```bash
cd firmware
pio run --environment esp32dev --target upload
pio device monitor  # optional: view serial output
```

### First-Time Flash vs. Updates

| Scenario | Command |
|----------|---------|
| First flash | `pio run --environment esp32dev --target upload` |
| Update firmware | Same command — OTA works after first flash |
| Erase & reflash | `pio run --environment esp32dev --target erase && pio run --environment esp32dev --target upload` |
| Build only (no flash) | `pio run --environment esp32dev` |
| View build output | `pio run --environment esp32dev --target upload --target monitor` |

---

## Method 2: ESP-IDF (Full Toolchain)

Use this if you want the full ESP-IDF toolchain for advanced debugging.

### Prerequisites

```bash
# Install ESP-IDF 5.x
curl -fsSL https://dl.espressif.com/dl/esp-idf/esp-idf-v5.2.2.zip -o esp-idf.zip
unzip -q esp-idf.zip -d ~/
rm esp-idf.zip

# Or clone:
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf
git checkout v5.2.2
git submodule update --init --recursive

# Install ESP-IDF tools
cd ~/esp/esp-idf
./install.sh esp32s3
```

### Configure & Flash

```bash
cd firmware
source ~/esp/esp-idf/export.sh    # Linux/macOS
# or: call %USERPROFILE%\esp\esp-idf\export.bat   # Windows

# Set target
idf.py set-target esp32s3

# Configure (optional — defaults work)
idf.py menuconfig

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

> **Tip:** Replace `/dev/ttyUSB0` with your serial port (`COM3`, `/dev/ttyACM0`, etc.)

### ESP-IDF Build Commands

| Command | Description |
|---------|-------------|
| `idf.py build` | Compile firmware |
| `idf.py -p <port> flash` | Flash firmware |
| `idf.py -p <port> monitor` | View serial output |
| `idf.py -p <port> flash monitor` | Flash + view output |
| `idf.py erase-flash` | Wipe flash memory |
| `idf.py app` | Build app binary only (for OTA) |
| `idf.py partition-table` | View partition layout |

---

## Method 3: Pre-Built Binary (No Compilation)

If you don't want to compile, use the pre-built OTA binary.

### Download Latest Firmware

```bash
# From your JARVIS Core server (after deploying OTA endpoint)
curl -O http://10.0.0.74:8080/ota/jarvis-device.bin

# Or from GitHub releases (coming soon)
curl -O https://github.com/Zebratic/jarvis-device/releases/latest/download/jarvis-device.bin
```

### Flash via esptool

```bash
# Install esptool
pip install esptool

# Find your serial port (Linux):
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null

# Find your serial port (macOS):
ls /dev/cu.usbserial* /dev/cu.usbmodem* 2>/dev/null

# Flash the binary
esptool.py \
  --chip esp32s3 \
  --port /dev/ttyUSB0 \
  --baud 921600 \
  write_flash \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0x10000 jarvis-device.bin \
  0x200000 firmware/spiffs.bin
```

> **Note:** You need to build the firmware at least once to generate `firmware/bootloader.bin` and `firmware/partitions.bin`. After that, only the main `jarvis-device.bin` changes.

---

## Method 4: Over-the-Air (OTA) Updates

After the first flash, update firmware wirelessly:

```bash
# Trigger OTA from JARVIS Core
curl -X POST http://10.0.0.74:8080/api/devices/ota \
  -H "Content-Type: application/json" \
  -d '{"device_id": "living-room", "url": "http://10.0.0.74:8080/ota/jarvis-device.bin"}'

# Or trigger via MQTT (device listens on its command topic):
mosquitto_pub -h 10.0.0.7 -t "home/jarvis-device/living-room/cmd" \
  -m '{"cmd":"ota"}'
```

The device checks for updates every 6 hours automatically. It also polls on boot.

---

## Verifying the Flash

After flashing, the device should:

1. **Print boot logs** to serial (115200 baud):
   ```
   I (xxx) jarvis-device: JARVIS Device v1.0.0 booting...
   I (xxx) wifi: Connecting to YourWiFiSSID...
   I (xxx) wifi: Connected! IP: 192.168.1.x
   I (xxx) mqtt: Connecting to 10.0.0.7:1883...
   I (xxx) mqtt: Connected, subscribed to home/jarvis-device/living-room/cmd
   I (xxx) ws: Connecting to ws://10.0.0.74:8080/ws/voice
   I (xxx) ws: Connected
   ```

2. **Publish online status** to MQTT:
   ```bash
   mosquitto_sub -h 10.0.0.7 -t "home/jarvis-device/living-room/avail"
   # Should print: online
   ```

3. **Show LED animation** — slow blue pulse when idle

---

## USB Driver Issues

If your computer doesn't detect the ESP32-S3 serial port:

| OS | Fix |
|----|-----|
| **Linux** | Install CH340 driver: `sudo apt install linux-modules-ch340` or `pip install esptool` |
| **macOS** | CH340 drivers usually work automatically. If not: `sudo kextload /Library/Extensions/ch34x.kext` |
| **Windows** | Install [CH340 driver](https://www.winaoe.org/display/CH340+DOWNLOAD) |
| **No port?** | Try a different USB-C cable — some cables are charge-only |

---

## Board Pinout Reference

```
ESP32-S3-DevKitC-1 Pinout (top view, USB-C left):

    [3V3] [GND]  [GPIO1] [GPIO2]  [GPIO3] [GPIO4]
    [GPIO5] [GPIO6] [GPIO7] [GPIO8] [GPIO9] [GPIO10]
    [GPIO11] [GPIO12] [GPIO13] [GPIO14] [GPIO15] [GPIO16]
    [GPIO17] [GPIO18] [GPIO19] [GPIO20] [GPIO21] [GPIO48]

    [BOOT]  [EN/RST]
    [USB-C]
```

**Firmware uses:**
- GPIO4 → INMP441 SD (I2S data)
- GPIO5 → INMP441 SCK (I2S clock)
- GPIO6 → INMP441 WS (I2S word select)
- GPIO48 → WS2812B LED data
- GPIO1 → I2C SDA (BME280)
- GPIO2 → I2C SCL (BME280)

---

## Partition Table

The firmware uses a custom partition table to leave room for OTA:

| Name | Offset | Size | Description |
|------|--------|------|-------------|
| nvs | 0x9000 | 0x5000 | WiFi credentials, config |
| otadata | 0xe000 | 0x2000 | OTA boot data |
| app0 | 0x10000 | 0x3B0000 | Main app (~3.8MB) |
| app1 | 0x3C0000 | 0x3B0000 | OTA partition |
| spiffs | 0x770000 | 0x890000 | SPIFFS (~8.7MB) |

**Total flash required: 16MB (N8R8 board recommended).**
