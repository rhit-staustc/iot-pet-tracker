# IoT Pet Tracker

A real-time GPS and motion tracking system for pets using Heltec WiFi LoRa V4 boards.

## Development

### Setup

1. **Install arduino-cli** from https://arduino.cc/en/software

2. **Add Heltec board support:**
   ```bash
   arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/Heltec-Aaron-Lee/WiFi_Kit_series/master/package_heltec_esp32_index.json
   arduino-cli core update-index
   arduino-cli core install Heltec-esp32:esp32
   ```

3. **Verify installation:**
   ```bash
   arduino-cli board list
   ```

### Building & Flashing

Use the build scripts in `scripts/` directory. Default COM port is **COM3** (configurable).

**Quick build and upload:**
```bash
# Build and flash Base Station
./scripts/build.sh build-base

# Build and flash Tracker Node
./scripts/build.sh build-tracker COM4  # optional: specify different COM port
```

**Individual commands:**
```bash
./scripts/build.sh compile-base           # Compile Base Station only
./scripts/build.sh upload-base [PORT]     # Upload Base Station
./scripts/build.sh compile-tracker        # Compile Tracker Node only
./scripts/build.sh upload-tracker [PORT]  # Upload Tracker Node
./scripts/build.sh monitor [PORT] [BAUD]  # Monitor serial output (default: COM3, 115200 baud)
```

**Using environment variables:**
```bash
export COM_PORT=COM4
./scripts/build.sh build-base  # Will use COM4
```

### Project Structure

See [CLAUDE.md](CLAUDE.md) for detailed architecture, hardware specifications, and implementation details.

```
Production/
  Base-Station/       - WiFi hub that receives LoRa packets and serves web dashboard
  Tracker-Node/       - GPS + LoRa tracker firmware
scripts/
  build.sh           - Main build script (compile, upload, monitor)
  compile-*.sh       - Individual compile scripts
  upload-*.sh        - Individual upload scripts
  monitor.sh         - Serial monitor
```

### Common Tasks

**Flash a different tracker ID:**
Edit `DEVICE_ID` in [Production/Tracker-Node/Tracker-Node.ino:38](Production/Tracker-Node/Tracker-Node.ino#L38) (0, 1, 2, or 3), then rebuild.

**Change LoRa frequency:**
Update LoRa `#define`s in both sketches (lines 18-26).

**Monitor serial output:**
```bash
./scripts/build.sh monitor COM3 115200
```

### Troubleshooting

**"Platform Heltec-esp32:esp32 not found"**
- Run the board setup commands under Setup → Add Heltec board support

**"arduino-cli not found"**
- Install from https://arduino.cc/en/software
- Ensure it's in your PATH

**OLED display not working**
- Check I2C connections (SDA/SCL pins)
- Verify display is enabled with `Vext` power line
- See [CLAUDE.md](CLAUDE.md) for OLED debugging

For more details, see [CLAUDE.md](CLAUDE.md).
