#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COM_PORT="${COM_PORT:-COM3}"

check_arduino_cli() {
  if ! command -v arduino-cli &> /dev/null; then
    echo "Error: arduino-cli not found"
    echo ""
    echo "Please install arduino-cli from: https://arduino.cc/en/software"
    exit 1
  fi
}

usage() {
  echo "Usage: $0 <command> [options]"
  echo ""
  echo "Commands:"
  echo "  compile-base              Compile Base Station"
  echo "  upload-base [PORT]        Upload Base Station (default: COM3)"
  echo "  compile-tracker           Compile Tracker Node"
  echo "  upload-tracker [PORT]     Upload Tracker Node (default: COM3)"
  echo "  monitor [PORT] [BAUD]     Monitor serial (default: COM3, 115200 baud)"
  echo "  build-base [PORT]         Compile and upload Base Station"
  echo "  build-tracker [PORT]      Compile and upload Tracker Node"
  echo ""
  echo "Example:"
  echo "  $0 build-base              # Compile and upload base to COM3"
  echo "  $0 monitor COM4            # Monitor COM4"
  exit 1
}

if [ -z "$1" ]; then
  usage
fi

check_arduino_cli

case "$1" in
  compile-base)
    bash "$SCRIPT_DIR/compile-base.sh"
    ;;
  upload-base)
    bash "$SCRIPT_DIR/upload-base.sh" "${2:-$COM_PORT}"
    ;;
  compile-tracker)
    bash "$SCRIPT_DIR/compile-tracker.sh"
    ;;
  upload-tracker)
    bash "$SCRIPT_DIR/upload-tracker.sh" "${2:-$COM_PORT}"
    ;;
  monitor)
    bash "$SCRIPT_DIR/monitor.sh" "${2:-$COM_PORT}" "${3:-115200}"
    ;;
  build-base)
    bash "$SCRIPT_DIR/compile-base.sh" && bash "$SCRIPT_DIR/upload-base.sh" "${2:-$COM_PORT}"
    ;;
  build-tracker)
    bash "$SCRIPT_DIR/compile-tracker.sh" && bash "$SCRIPT_DIR/upload-tracker.sh" "${2:-$COM_PORT}"
    ;;
  *)
    usage
    ;;
esac
