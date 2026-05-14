#!/bin/bash

COM_PORT="${1:-COM3}"
BAUD_RATE="${2:-115200}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_FILE="monitor_${TIMESTAMP}.txt"

echo "Monitoring serial output on $COM_PORT at $BAUD_RATE baud..."
echo "Output will be saved to: $OUTPUT_FILE"
echo "(Press Ctrl+C to stop and save)"

trap "echo ''; echo 'Monitor stopped. Output saved to: $OUTPUT_FILE'; exit 0" SIGINT

arduino-cli monitor -p "$COM_PORT" --config baudrate="$BAUD_RATE" | tee "$OUTPUT_FILE"
