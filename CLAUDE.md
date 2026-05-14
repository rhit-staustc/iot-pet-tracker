# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**IoT Pet Tracker** is a real-time GPS and motion tracking system for pets. It consists of:
- **Tracker Nodes**: Small wearable devices with GPS and LoRa radio that collect and transmit pet location/motion data
- **Base Station**: WiFi-enabled hub that receives LoRa packets and serves a live web dashboard showing all trackers in real-time

All code is Arduino sketches running on Heltec WiFi LoRa V4 (ESP32-S3) boards.

## Hardware

**Heltec WiFi LoRa V4** specifications:
- ESP32-S3 microcontroller
- 915 MHz LoRa radio
- Built-in WiFi and BLE
- 128x64 OLED display
- ~320KB RAM, 3.3MB flash

Both base station and tracker nodes use the same hardware with different firmware.

## Architecture

### Base Station (`Production/Base-Station/Base-Station.ino`)

- **LoRa receiver**: Listens on 915 MHz for GPS packets from tracker nodes
- **Web server**: Built-in ESP32 WebServer (port 80) serves a responsive HTML dashboard
- **SSE updates**: Live data streaming via Server-Sent Events (`/events` endpoint)
- **OLED display**: Shows current WiFi IP and listening status
- **Command interface**: Serial console allows manual GPS reset commands

**Key structs:**
- `GPSPacket`: 14 bytes transmitted by trackers (status, battery%, lat, lon, speed, heading)
- `TrackerState`: Maintains last packet + RSSI/SNR for each tracker ID (0-3)

### Tracker Nodes (`Production/Tracker-Node/Tracker-Node.ino`)

- **GPS module**: u-blox GPS (39=RX, 38=TX) reads location and speed
- **Battery monitor**: ADC on pin 37 tracks voltage; calculated as percentage (3500-4200 mV)
- **LoRa transmitter**: Sends GPSPacket every 500ms with device ID embedded in status byte
- **IMU (MPU6500)**: Reads accelerometer and gyro on I2C (Wire1, pins 6/7) for behavior classification
- **Behavior classification**: Motion analysis (acceleration, speed) for idle/walking/running/sitting states

## Build & Development Commands

**Use PowerShell for all commands.** `arduino-cli` is in your PATH; the monitor script is in `scripts/monitor-capture.ps1`.

### Compilation

```powershell
# Compile Base Station
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Base-Station

# Compile Tracker Node
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Tracker-Node
```

### Upload to Board

```powershell
# Upload Base Station (default COM3)
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Base-Station

# Upload Tracker Node to different COM port
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM4 Production/Tracker-Node
```

### Serial Monitoring

```powershell
# Capture serial output for 30 seconds (default COM3, 115200 baud)
powershell -ExecutionPolicy Bypass -File ./scripts/monitor-capture.ps1 -DurationSeconds 30

# Specify different port or baud rate
powershell -ExecutionPolicy Bypass -File ./scripts/monitor-capture.ps1 -ComPort COM4 -BaudRate 115200 -DurationSeconds 30
```

The script saves timestamped output (e.g., `monitor_20260513_143028.txt`) that can be read with `Read monitor_*.txt`.

### Quick Reference (Compile + Upload)

```powershell
# Compile and upload Base Station
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Base-Station; `
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Base-Station

# Compile and upload Tracker Node
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Tracker-Node; `
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Tracker-Node
```

See [README.md](README.md) for setup instructions and troubleshooting.

## Key Implementation Details

### LoRa Configuration

All devices use identical LoRa settings (915 MHz, SF10, BW=125kHz). The tracker transmits, base station listens. No two-way LoRa communication yet (only serial commands on base).

**Packet format:**
- Status byte encodes tracker ID (bits 0-1), fix validity (bit 2), data freshness (bit 3)
- Speed in mph × 100 (uint16_t), course in degrees × 100 (uint16_t) for fixed-point precision

### WiFi Setup

Base station connects to `RHIT-OPEN` (open network, no password). Edit lines 9-10 in Base-Station.ino to change SSID/password.

Web dashboard is served at `http://<device-ip>/` and displays all 4 tracker cards with live SSE updates.

### IMU and I2C

Tracker node uses the MPU6500 IMU on Wire1 (pins 6/7). Wire1 is used instead of the default Wire because `Mcu.begin()` (the Heltec board init) calls `Wire.begin(SDA_OLED, SCL_OLED)` via the `HT_SSD1306Wire` OLED library, reconfiguring Wire to the OLED pins (17/18) and making it unusable for the IMU. Wire1 avoids this conflict. Additionally, transmitting at 28 dBm can corrupt in-flight I2C transactions, so the code performs bus recovery (`Wire1.end()` / `Wire1.begin()`) after each LoRa TX to clear any lockup. The IMU is calibrated on startup (5-second hold-still period) and provides accelerometer/gyro data for behavior classification.

## Repository Structure

```
Production/
  Base-Station/
    Base-Station.ino       (main base station code)
    WebContent.h           (unused; can be removed)
  Tracker-Node/
    Tracker-Node.ino       (main tracker firmware)

Testing/
  GPS-Testing/            (GPS module validation)
  LoRa-Testing/           (LoRa range/reliability tests)
  GPS-and-IMU-Testing/    (combined sensor testing)
  Behavior-Classification-Testing/
  compile.sh, upload.sh, monitor.sh (development scripts)

documents/                 (progress reports, presentations)
datasheets/                (hardware datasheets)
```

## Common Development Tasks

**Adding a new tracker:** Tracker ID is hardcoded in `DEVICE_ID` variable (Tracker-Node.ino line 30). Change to 0, 1, 2, or 3 and recompile.

**Changing LoRa frequency/config:** All LoRa #defines are identical in both sketches (lines 17-24). Edit both files to keep them in sync.

**Debugging packet reception:** Serial prints all received packets with ID, battery, coordinates, RSSI, SNR. Monitor via serial console.

**Testing web dashboard offline:** Base station will start WiFi and serve the dashboard even without active trackers. Tracker cards show "Offline" until data arrives.

## Known Issues & Branches

- **`feature/imu-no-oled`**: Active development branch. Removes OLED display, integrates MPU6500 IMU on Wire1 (pins 6/7) with behavior classification. Uses I2C bus recovery (Wire1.end/begin) before each transaction. Not yet merged to main.
- **`feature/web-server`**: Live dashboard via SSE. Not yet merged to main.
- **`mpu`**: Earlier IMU (MPU-9250) integration branch; superseded by `feature/imu-no-oled`.

Main branch is the stable release with GPS + LoRa communication working. IMU and behavior classification are on `feature/imu-no-oled`.

## Libraries & Dependencies

Arduino IDE libraries (install via Library Manager):
- Heltec ESP32 board definitions (https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series)
- TinyGPS++ (for GPS parsing)
- Built-in: WiFi, WebServer, Wire (I2C)

No external dependencies for web server; HTML/CSS/JS is embedded in PROGMEM.
