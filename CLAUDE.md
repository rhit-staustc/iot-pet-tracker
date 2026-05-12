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
- **Behavior classification**: Motion analysis (acceleration, heading change) on a separate `behavior-classification` branch
- **OLED display**: Shows GPS fix status, battery, coordinates, and signal metrics

## Build & Development Commands

**Always use Bash for shell commands, not PowerShell.** Use the scripts in `scripts/` directory for all build operations.

### Quick Build

```bash
# Build and flash Base Station
./scripts/build.sh build-base

# Build and flash Tracker Node
./scripts/build.sh build-tracker

# Specify different COM port
./scripts/build.sh build-tracker COM4
```

### Individual Operations

```bash
# Compile only
./scripts/build.sh compile-base
./scripts/build.sh compile-tracker

# Upload only
./scripts/build.sh upload-base [PORT]
./scripts/build.sh upload-tracker [PORT]

# Monitor serial output (default: COM3, 115200 baud)
./scripts/build.sh monitor [PORT] [BAUD]
```

### Manual Commands (if needed)

All scripts use `arduino-cli`. For direct command usage:

```bash
# Compile
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Base-Station

# Upload
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Base-Station

# Monitor
arduino-cli monitor -p COM3 --config baudrate=115200
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

### OLED Display

Both devices use a shared `display` object (I2C at 0x3c). Base station updates after each LoRa RX; tracker updates periodically. Display refreshes are non-blocking.

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

- **`behavior-classification`**: Implements motion detection via IMU/GPS analysis. Stable but not merged to main.
- **`mpu`**: IMU (MPU-9250) integration branch.
- **`feature/web-server`**: Live dashboard via SSE; considered the latest feature branch.

Main branch is the stable release with GPS + LoRa communication working.

## Libraries & Dependencies

Arduino IDE libraries (install via Library Manager):
- Heltec ESP32 board definitions (https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series)
- TinyGPS++ (for GPS parsing)
- Built-in: WiFi, WebServer, Wire (I2C)

No external dependencies for web server; HTML/CSS/JS is embedded in PROGMEM.
