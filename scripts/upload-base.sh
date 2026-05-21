#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="$REPO_ROOT/Production/Base-Station"
COM_PORT="${1:-COM3}"

echo "Uploading Base Station to $COM_PORT..."
cd "$SKETCH_DIR"
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p "$COM_PORT" .

echo "✓ Base Station uploaded successfully"
