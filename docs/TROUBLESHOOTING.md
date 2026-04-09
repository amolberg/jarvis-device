# JARVIS Device — Troubleshooting Guide

Common problems and solutions for the JARVIS DIY room node.

---

## Diagnostic Commands

Before troubleshooting, gather info about what's happening:

```bash
# Check if device serial is detected
ls /dev/ttyUSB* /dev/ttyACM*     # Linux
ls /dev/cu.usbserial*            # macOS
python -m esptool chip_id        # Verify chip

# Test MQTT connectivity
mosquitto_sub -h 10.0.0.7 -v -t "home/jarvis-device/#"

# Check JARVIS Core is running
curl http://10.0.0.74:8080/api/health

# Check WebSocket endpoint
curl -i http://10.0.0.74:8080/ws/voice  # Should give 426 upgrade required

# Check Home Assistant MQTT
mosquitto_sub -h 10.0.0.7 -v -t "homeassistant/#"
```

---

## Problem 1: Device Won't Flash

**Symptoms:** `esptool` can't detect the chip, or flashing fails with timeout.

### Causes & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `A fatal error occurred: Failed to connect to ESP32` | Device not in bootloader mode | Hold **BOOT** button, tap **EN/RST**, release BOOT, then flash |
| `esptool: error: could not open /dev/ttyUSB0` | Permission denied | `sudo usermod -a -G dialout $USER` then log out/in |
| `Timed out waiting for packet header` | Bad USB cable | Use a data-capable USB-C cable (not charge-only) |
| `Write timeout` | Wrong baud rate | Try `--baud 460800` instead of `921600` |
| `Wrong chip. Found ESP32-S3, but the build is for ESP32` | Wrong target | Run `idf.py set-target esp32s3` or `pio run --environment esp32s3` |
| `insufficient flash size` | Board has too little flash | Use ESP32-S3 with 8MB+ flash (N8R8 recommended) |

### Force Bootloader Mode

1. Hold the **BOOT** button on the ESP32-S3
2. While holding BOOT, press and release **EN/RST**
3. Release BOOT
4. Flash within 10 seconds

```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 115200 \
  write_flash 0x1000 firmware/bootloader.bin 0x8000 firmware/partitions.bin 0x10000 firmware/firmware.bin
```

---

## Problem 2: WiFi Won't Connect

**Symptoms:** Serial shows `wifi: Disconnected, reason: (xx)` or device keeps rebooting.

### Causes & Fixes

| Error / Symptom | Cause | Fix |
|----------------|-------|-----|
| `reason: (201)` | Wrong password | Check `WIFI_PASSWORD` in `config.h` |
| `reason: (15)` | Network full | Router may have max client limit — check router |
| `reason: (x00)` | Out of range | Move device closer to router |
| `reason: (x03)` | SSID not found | Verify `WIFI_SSID` matches exactly (case-sensitive, no spaces) |
| Static IP doesn't work | Wrong IP/range | Check `JARVIS_CORE_HOST` is reachable from the device's subnet |
| `wifi: Connecting...` forever | Router has client isolation | Disable AP isolation on router, or add device MAC to DMZ |

### Static IP (Optional)

To use a static IP instead of DHCP, add to `firmware/src/main.cpp`:

```cpp
// In wifi_connect():
esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
esp_netif_dhcpc_stop(netif);

esp_netif_ip_info_t ip_info = {
    .ip = {.addr = esp_ip4addr_aton("192.168.1.100")},
    .gw = {.addr = esp_ip4addr_aton("192.168.1.1")},
    .netmask = {.addr = esp_ip4addr_aton("255.255.255.0")},
};
esp_netif_set_ip_info(netif, &ip_info);
```

---

## Problem 3: MQTT Won't Connect

**Symptoms:** Serial shows `mqtt: Failed to connect`, or device publishes to `avail` but nothing else.

### Causes & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `mqtt: Connection refused` | Wrong broker IP | Check `MQTT_SERVER` in `config.h` |
| `mqtt: Connection refused (5)` | Authentication failed | Set `MQTT_USERNAME` and `MQTT_PASSWORD` in `config.h` |
| `mqtt: Connection refused (4)` | Not authorized | Check HA MQTT add-on username/password |
| Mosquitto not running on HA | HA MQTT add-on not installed | Install **Mosquitto broker** add-on in Home Assistant |

### Verify MQTT from CLI

```bash
# Test MQTT connectivity (publish a test message)
mosquitto_pub -h 10.0.0.7 -t "test/jarvis" -m "hello"

# Subscribe to all JARVIS device topics
mosquitto_sub -h 10.0.0.7 -v -t "home/jarvis-device/#"

# Check if HA MQTT add-on is running
curl -s http://10.0.0.7:8123/api/hassio/addons \
  -H "Authorization: Bearer $HA_TOKEN" | jq '.data[] | select(.name=="mosquitto")'
```

### Enable MQTT on Home Assistant

1. Go to **Settings → Add-ons → Add-on Store**
2. Install **Mosquitto broker**
3. Configure with username/password
4. Start the add-on
5. In **Settings → Devices & Services → Integrations**, add **MQTT** integration
6. Click **CONFIGURE** on MQTT → check "Enable discovery"

---

## Problem 4: WebSocket Won't Connect

**Symptoms:** `ws: Failed to connect`, `ws: Connection reset`, or device boots but voice doesn't work.

### Causes & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `ws: DNS failed` | `JARVIS_CORE_HOST` unreachable | Check IP is correct and device can ping it |
| `ws: Connection refused` | JARVIS Core not running | Start JARVIS Core: `cd /root/jarvis-dev/jarvis-core && source venv/bin/activate && python -m core.api.main &` |
| `ws: TLS handshake failed` | Trying HTTPS on HTTP | Don't use `https://` prefix — WebSocket should be `ws://` |
| `ws: Connection reset` | JARVIS Core crashed | Check JARVIS Core logs: `curl http://10.0.0.74:8080/api/health` |
| WS connects but no audio | JARVIS Core too busy | Check LLM is ready: health endpoint shows `llm_ready: true` |

### Verify WebSocket Endpoint

```bash
# Test JARVIS Core WebSocket (will give upgrade error via HTTP, but confirms it's listening)
curl -i http://10.0.0.74:8080/ws/voice

# Test if JARVIS Core is responding at all
curl http://10.0.0.74:8080/api/health | python -m json.tool

# Check JARVIS Core process
ps aux | grep jarvis | grep -v grep
```

---

## Problem 5: No Audio / Poor Audio Quality

**Symptoms:** Device connects, voice is sent, but JARVIS can't hear anything or responses are garbled.

### Check Microphone

```bash
# In serial monitor, check for VAD output:
# You should see something like this when speaking near the mic:
# [VAD] RMS: 2345 — SPEAKING (or IDLE if quiet)
```

| Symptom | Cause | Fix |
|---------|-------|-----|
| VAD always shows 0 | INMP441 not wired correctly | Check I2S wiring — WS, SCK, SD pins must match `config.h` |
| VAD always max | Pin short or noise | Check for wire bridges on GPIO4/5/6 |
| Audio clicks/pops | I2S timing jitter | Normal for ESP32 — increase `VAD_MIN_CHUNKS` in `config.h` |
| Only loud sounds trigger | VAD threshold too high | Lower `VAD_THRESHOLD` in `config.h` (default 500, try 300) |

### I2S Wiring Check

Verify connections match `config.h`:

| INMP441 Pin | ESP32 GPIO | config.h |
|-------------|-----------|----------|
| WS (word select) | GPIO6 | `I2S_WS_PIN 6` |
| SCK (clock) | GPIO5 | `I2S_SCK_PIN 5` |
| SD (data out) | GPIO4 | `I2S_SD_PIN 4` |
| L/R (select) | GND | — |
| VCC | 3V3 | — |
| GND | GND | — |

**Common mistake:** INMP441 L/R pin must be GND (not 3V3) for left-channel I2S output.

### VAD Tuning

```c
// In config.h — tune these values:
#define VAD_THRESHOLD     500   // Lower = more sensitive (try 300-200)
#define VAD_MIN_CHUNKS    3     // Consecutive chunks before triggering
#define VAD_SILENCE_CHUNKS 8   // Silence chunks before stopping stream
```

Rebuild and reflash after changing these values.

---

## Problem 6: LED Ring Not Working

**Symptoms:** Device works but LEDs don't light up.

| Symptom | Cause | Fix |
|---------|-------|-----|
| No LEDs light at all | Data pin wrong | Check GPIO48 connects to DI on WS2812B |
| Only first LED lights | Signal too weak | Add a 330Ω resistor between GPIO48 and DI |
| Colors wrong | RGB vs GRB | WS2812B is GRB — if colors look swapped, check LED library |
| LEDs flicker | Power drop | WS2812B needs 5V — power ring separately if dimming |

**Power WS2812B ring:** Connect ring's 5V to USB 5V (max 2A draw). The ESP32 can't supply enough current for 24 LEDs at full brightness.

---

## Problem 7: Device Goes to Sleep Too Fast / Never Sleeps

```c
// In config.h:
#define DEEP_SLEEP_TIMEOUT_S 300  // 5 minutes — increase or set to 0 to disable
```

To disable deep sleep entirely, set `DEEP_SLEEP_TIMEOUT_S = 0` and rebuild.

---

## Problem 8: Device Reboots in a Loop

Check serial output for the crash reason. Common causes:

| Boot Log | Cause | Fix |
|----------|-------|-----|
| `Guru Meditation Error` | Crash in code | Flash with `debug` logging: enable `CONFIG_ESP_SYSTEM_CHECK_INT_LEVEL_DEFAULT` |
| `abort()` | Out of memory | Reduce `AUDIO_CHUNK_MS` or `LED_COUNT` |
| `Brownout detector` | Power supply too weak | Use a stronger USB power supply (5V 2A minimum) |
| `rst:0x10 (RTCWDT_RTC_RESET)` | Watchdog timeout | Something is blocking too long — check MQTT callback |
| `rst:0x3 (SW_RESET)` | Software reset | Caused by `esp_restart()` — check the trigger |

### Enable Core Dump

```bash
# In ESP-IDF:
idf.py menuconfig → Component config → ESP32S3-specific → Core dump → UART

# After crash, read core dump:
python $IDF_PATH/components/espcoredump/espcoredump.py \
  -p /dev/ttyUSB0 coredump_info
```

---

## Problem 9: Home Assistant Doesn't See the Device

1. **MQTT Discovery not enabled:** In HA → Settings → Devices & Services → MQTT → Configure → Enable Discovery
2. **Wrong discovery prefix:** Default is `homeassistant/` — check your HA config
3. **Device not auto-discovered:** Manually add in HA UI:

```yaml
# In configuration.yaml
mqtt:
  sensor:
    - name: "JARVIS Device Living Room Status"
      state_topic: "home/jarvis-device/living-room/state"
      json_attributes_topic: "home/jarvis-device/living-room/state"
```

---

## Problem 10: Factory Reset

To wipe all WiFi/MQTT credentials and start fresh:

```bash
# Via ESP-IDF:
idf.py -p /dev/ttyUSB0 erase-flash

# Via esptool:
esptool.py --chip esp32s3 --port /dev/ttyUSB0 erase_flash

# Via MQTT:
mosquitto_pub -h 10.0.0.7 -t "home/jarvis-device/living-room/cmd" -m '{"cmd":"factory_reset"}'
```

After erasing, the device will re-broadcast its WiFi AP for reconfiguration (if AP mode is enabled in firmware).

---

## Getting Help

1. **Serial output is your best friend** — always start with `pio device monitor` or `idf.py monitor`
2. **Check JARVIS Core logs** — `curl http://10.0.0.74:8080/api/health` and JARVIS Core console
3. **Check Home Assistant logs** — Settings → System → Logs → MQTT
4. **Open an issue** at https://github.com/Zebratic/jarvis-device/issues with:
   - Board model (ESP32-S3 DevKitC-1 N8R8?)
   - Serial output (paste the last 50 lines from serial monitor)
   - Steps to reproduce
   - What you've already tried

---

## Quick Reference: Log Patterns

| Log Pattern | Meaning |
|-------------|---------|
| `I (xxx) wifi: Connected!` | WiFi OK |
| `I (xxx) mqtt: Connected` | MQTT OK |
| `I (xxx) ws: Connected` | WebSocket OK |
| `[VAD] RMS: xxxx` | Mic working |
| `E (xxx) wifi: Disconnected` | WiFi failed |
| `E (xxx) mqtt: Connection refused` | MQTT failed |
| `E (xxx) ws: Failed to connect` | WebSocket failed |
| `Guru Meditation Error` | Code crash — need debug |
