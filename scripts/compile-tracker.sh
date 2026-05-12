#!/bin/bash
set -e

if ! command -v arduino-cli &> /dev/null; then
  echo "Error: arduino-cli not found"
  echo "Please install from: https://arduino.cc/en/software"
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="$REPO_ROOT/Production/Tracker-Node"

echo "Compiling Tracker Node..."
cd "$SKETCH_DIR"
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc .

echo "✓ Tracker Node compiled successfully"
