#!/bin/bash

COM_PORT="${1:-COM3}"
BAUD_RATE="${2:-115200}"

echo "Monitoring serial output on $COM_PORT at $BAUD_RATE baud..."
echo "(Press Ctrl+C to exit)"

arduino-cli monitor -p "$COM_PORT" --config baudrate="$BAUD_RATE"
