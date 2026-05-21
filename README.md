# IoT Pet Tracker

A real-time GPS and motion tracking system for pets using Heltec WiFi LoRa V4 boards. Tracker nodes report GPS location, speed, and IMU-based behavior (idle/walking/running/sitting) over LoRa to a base station that serves a live web dashboard.

## Setup

1. **Install arduino-cli** from https://arduino.cc/en/software

2. **Add Heltec board support:**
   ```powershell
   arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/Heltec-Aaron-Lee/WiFi_Kit_series/master/package_heltec_esp32_index.json
   arduino-cli core update-index
   arduino-cli core install Heltec-esp32:esp32
   ```

3. **Verify installation:**
   ```powershell
   arduino-cli board list
   ```

## Building & Flashing

Default COM port is **COM3**. Use PowerShell for all commands.

**Compile:**
```powershell
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Base-Station
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Tracker-Node
```

**Upload:**
```powershell
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Base-Station
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Tracker-Node
```

**Monitor serial output (30 seconds):**
```powershell
powershell -ExecutionPolicy Bypass -File ./scripts/monitor-capture.ps1 -DurationSeconds 30
```

## Project Structure

See [CLAUDE.md](CLAUDE.md) for detailed architecture, hardware specifications, and implementation details.

```
Production/
  Base-Station/       - WiFi hub that receives LoRa packets and serves web dashboard
  Tracker-Node/       - GPS + IMU + LoRa tracker firmware
Testing/
  I2C-Scanner/        - Diagnostic sketch to locate I2C devices (run before Mcu.begin())
  GPS-Testing/        - GPS module validation
  LoRa-Testing/       - LoRa range/reliability tests
scripts/
  monitor-capture.ps1 - Captures timestamped serial output to scripts/monitor-output/
```

## Common Tasks

**Flash a different tracker ID:**
Edit `DEVICE_ID` in [Production/Tracker-Node/Tracker-Node.ino:46](Production/Tracker-Node/Tracker-Node.ino#L46) (0, 1, 2, or 3), then recompile and upload.

**Enable debug output:**
Uncomment `#define DEBUG` near the top of [Tracker-Node.ino](Production/Tracker-Node/Tracker-Node.ino). Recompile and upload. Debug output includes IMU accel values, HeadOrient, calibration biases, and TX state transitions.

**Change LoRa frequency:**
Update the LoRa `#define`s in both sketches (identical values required in both).

## Troubleshooting

**"Platform Heltec-esp32:esp32 not found"**
- Run the board setup commands under Setup above

**"arduino-cli not found"**
- Install from https://arduino.cc/en/software and ensure it's in your PATH

**IMU reads frozen or wrong after first TX**
- This is caused by RF-induced I2C bus lockup. The firmware recovers automatically via `needsBusRecovery` flag — see [CLAUDE.md](CLAUDE.md) for details.

For full implementation details, see [CLAUDE.md](CLAUDE.md).
