# JARVIS Device — Assembly Guide

Step-by-step instructions to build your own JARVIS room node from scratch. Total build time: 1-2 hours. Total cost: ~$18-25.

---

## Prerequisites

### Tools Required

| Tool | Approx Cost | Notes |
|------|------------|-------|
| Soldering iron (25-30W) | ~$15 | Essential for INMP441 wiring |
| Solder wire (0.8mm rosin core) | ~$5 | Lead-free or 60/40 tin/lead |
| Wire cutters / strippers | ~$10 | Dedicated tool or combo pliers |
| Phillips screwdriver (PH0) | ~$5 | For case screws |
| Multimeter | ~$20 | Optional but highly recommended |
| Helping hands (alligator clips) | ~$8 | Holds parts while soldering |
| 3D printer (optional) | — | For the case |

### Parts Checklist

Print or screenshot this before ordering:

```
□ ESP32-S3-DevKitC-1 N8R8        (~$9)
□ INMP441 microphone module        (~$2)   ×3 recommended
□ WS2812B 24-LED ring             (~$2)
□ 100µF 16V electrolytic cap      ($0.10) ×2
□ 100nF ceramic cap (0805/0603)   ($0.05) ×3
□ Jumper wires / DuPont cables    (~$3)
□ USB-C cable                      (you probably have one)
□ USB power supply (5V 2A)        (you probably have one)
```

---

## Step 1 — Print the Case (Optional)

If you have a 3D printer:

1. Install [OpenSCAD](https://openscad.org) or use the web version
2. Open `hardware/case/jarvis-case.scad`
3. Preview with **F5**, export STL with **F7**
4. Slice with your preferred slicer (Cura, PrusaSlicer, Bambu Studio)
5. Print settings:
   - Layer height: 0.2mm
   - Infill: 15-20%
   - Walls: 3 perimeters minimum
   - No supports needed
   - Material: PLA or PETG

**Alternative:** Use a small project box from a hardware store (~50×80×20mm). The ESP32-S3 DevKitC-1 is about 25×50mm, so most enclosures will work.

---

## Step 2 — Prepare the ESP32-S3

### Check Your Board

The ESP32-S3-DevKitC-1 has:
- USB-C connector (for power + programming)
- 2×10 header pins (one side: 3V3, GND, GPIO4-6, GPIO1-2, GPIO48)
- Boot and Reset buttons
- RGB LED (onboard, for testing)

### Test It First

**Before soldering anything**, verify the board works:

1. Connect USB-C cable to your computer
2. You should see a new serial port (e.g., `/dev/ttyUSB0` on Linux, `COM3` on Windows)
3. Install Python + esptool:

```bash
pip install esptool
esptool --chip esp32s3 chip_id
```

4. You should see chip info printed. If not, install the [CH340 USB driver](https://www.winaoe.org/display/CH340+DOWNLOAD).

**Do not proceed** if the board doesn't enumerate on USB. Return/exchange it.

---

## Step 3 — Wire the INMP441 Microphone

The INMP441 is a digital I2S microphone. It requires 4 signal wires + power.

### Pin Identification

The INMP441 has 8 pins. Count carefully:

```
   ┌─────────────┐
 1 │ VDD         │
 2 │ GND         │
 3 │ LR          │  ← Leave unconnected or tie to GND for LEFT channel
 4 │ WS          │  ← Word Select (I2S word clock)
 5 │ SCK         │  ← Serial Clock (I2S bit clock)
 6 │ SD          │  ← Serial Data (the audio output)
 7 │ NC          │  ← Not connected
 8 │ VDD         │
   └─────────────┘
    (front view)
```

### Wiring

```
ESP32-S3              INMP441
────────────          ──────
GPIO4 (SD_DATA)  ──→  SD   (pin 6)
GPIO5 (SCK)      ──→  SCK  (pin 5)
GPIO6 (WS)       ──→  WS   (pin 4)
3.3V             ──→  VDD  (pins 1 & 8)
GND              ──→  GND  (pin 2)
GND              ──→  LR   (pin 3 — sets LEFT channel)
```

### Soldering Tips

1. **Tin the pad first** — add a tiny bit of solder to the INMP441 pad
2. **Tin the wire** — strip 3mm, twist strands, add solder
3. **Heat both** — hold iron on joint for 2-3 seconds max
4. **Don't overheat** — the INMP441 is sensitive to heat. Use a solder station with temperature control, keep it at 350°C max.

For beginners: practice on scrap wire first. The INMP441 pads are small (~1mm pitch). If you're not comfortable soldering, use the **ReSpeaker 2-Mic HAT** instead — it plugs in directly without soldering.

---

## Step 4 — Wire the WS2812B LED Ring

### Wiring

```
ESP32-S3              WS2812B Ring
────────────          ────────────
GPIO48 (LED_DIN) ──→  DIN  (Data In)
5V USB            ──→  5V   (both 5V pads on ring)
GND               ──→  GND  (both GND pads on ring)
```

**Critical:** The WS2812B ring requires **5V**, not 3.3V! The ring has a 5V input and level-shifts the data line internally. ESP32 GPIO48 at 3.3V is sufficient for the data signal.

### Power Stabilization

The LED ring can draw up to 1.4A at full white brightness. Add a **100µF capacitor** across the 5V and GND wires near the ring connector. This prevents brownout flashes when all LEDs turn on.

```text
 5V ──→ ┤├── 100µF ──→ GND
         └──┤
```

Also add **100nF ceramic capacitors** near:
- ESP32 power pins (3V3/GND)
- INMP441 power pins

---

## Step 5 — Wire the BME280 (Optional)

The BME280 is optional but adds ambient temperature, humidity, and pressure sensing. JARVIS can use this for room climate awareness.

### Wiring (I2C)

```
ESP32-S3              BME280
────────────          ──────
GPIO1 (SDA)      ──→  SDA
GPIO2 (SCL)      ──→  SCL
3.3V             ──→  VCC
GND              ──→  GND
```

Most BME280 modules have: VCC, GND, SDA, SCL (4 pins). Some also have SDO (leave unconnected for default I2C address 0x76).

---

## Step 6 — Mount in Case

### If Using the 3D Printed Case

1. **Base plate**: Place ESP32-S3 DevKitC-1 into the recess. The USB-C connector should align with the cutout on the right side.
2. **Mounting posts**: The case has 4 mounting posts with holes. Use M2×6mm screws to secure the ESP32 board.
3. **LED ring**: Place the WS2812B ring in the top recess. The ring has two sets of pads (input → output). Connect to the ESP32 side.
4. **Microphone**: The INMP441 sits inside the base. The hole on the front face allows sound through.
5. **Lid**: Press-fit the lid into the base. It has a slight interference fit.

### If Using a Project Box

1. Cut foam padding to line the box interior
2. Hot glue or double-sided tape to secure components
3. Ensure the mic has a clear path to sound (drill a small hole)
4. Route USB-C cable out of the box

---

## Step 7 — Flash the Firmware

### Option A: USB-C Direct Flash (Recommended)

1. Connect ESP32-S3 to your computer via USB-C
2. Set board to download mode:
   - Hold **BOOT** button
   - Press and release **RESET** button
   - Release **BOOT** button
3. Flash the firmware:

```bash
cd firmware
idf.py -p /dev/ttyUSB0 flash monitor
```

(Replace `/dev/ttyUSB0` with your serial port. On Windows: `COM3`, macOS: `/dev/cu.usbserial-*`)

### Option B: OTA Update (After First Flash)

After the initial firmware is flashed, update over WiFi:

1. The device connects to your WiFi network
2. Place new firmware binary at `http://<jarvis-core-ip>/ota/jarvis-device.bin`
3. Send MQTT command `{"cmd":"ota"}` to trigger update

### Option C: PlatformIO (Alternative)

```bash
pip install platformio
cd firmware
pio run --environment esp32dev --target upload
pio device monitor --baud 115200
```

---

## Step 8 — Configure

After flashing, connect to the device's serial monitor (115200 baud). You'll see:

```
JARVIS Device v1.0
====================
[INIT] Starting up...
[INIT] WiFi: scanning...
```

### Configure WiFi

The device will scan for networks and print them. To connect:

```
> wifi_connect MyNetwork MyPassword
```

Or use the configuration mode:

```
> config
SSID: MyNetwork
Password: MyPassword
JARVIS Core URL: http://10.0.0.74:8080
Device Name: Living Room
```

### Test It

```bash
# Check device is online
curl http://<jarvis-core-ip>:8080/api/health

# See connected devices
curl http://<jarvis-core-ip>:8080/api/devices | jq '. | length'
```

---

## Step 9 — First Voice Test

1. Open the JARVIS dashboard (http://your-server:3456)
2. Go to the **Voice** tab
3. Hold the microphone near you and speak
4. You should see:
   - Blue LED → listening
   - Purple LED → processing
   - Green LED → JARVIS speaking

---

## Troubleshooting

### Device won't boot / no serial output

- **Check power**: ESP32-S3 needs stable 5V via USB-C. Low-quality USB cables can drop voltage.
- **Try a different USB cable**: Some charge-only cables won't work.
- **Check for shorts**: A solder bridge between pins will prevent boot. Use a multimeter in continuity mode to check adjacent pins.
- **Factory reset**: `idf.py erase-flash` wipes everything and restores factory state.

### WiFi won't connect

- Verify SSID and password are correct (case-sensitive!)
- ESP32-S3 only supports 2.4GHz WiFi — NOT 5GHz
- Check signal strength. Move the device closer to the router during setup.
- For hidden SSIDs, the ESP may struggle. Try broadcasting the SSID temporarily.

### INMP441 produces no audio

- Check the **L/R** pin — it MUST be connected to GND for left-channel output
- Verify 3.3V power — the INMP441 draws ~2.5mA
- Test with: `curl -X POST http://<jarvis-core-ip>:8080/api/wake-word/test-audio`
- Inspect the I2S wiring under magnification. The pads are easy to bridge.

### LEDs flicker or turn wrong colors

- **Add the 100µF capacitor** across 5V/GND near the ring
- Check for loose connections on the DATA IN wire
- Reduce LED brightness in `config.h` if powering from weak USB
- Make sure you're using the **DIN** (data in) pad, not DOUT

### MQTT disconnects

- Home Assistant MQTT add-on must be running: http://10.0.0.7:8123 → Settings → Add-ons → Mosquitto broker
- Verify MQTT username/password in `config.h`
- Check `homeassistant` status in HA developer tools

---

## Next Steps

Once assembled and running:

1. Add more devices to other rooms (~$18 each)
2. Configure custom wake words via the dashboard
3. Set up automations in Home Assistant triggered by JARVIS
4. Tune the voice pipeline for your accent and environment

---

*Built with ❤️ by amolberg — April 2026*
