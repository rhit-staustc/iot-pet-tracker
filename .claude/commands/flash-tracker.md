Compile and upload the Tracker Node firmware to the board.

1. Compile `Production/Tracker-Node` using:
   ```
   arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Tracker-Node
   ```
2. If compilation succeeds, upload to the COM port specified by the user (default: COM3):
   ```
   arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Tracker-Node
   ```
3. Report success or any errors. The serial monitor will start automatically after upload via the post-upload hook.

If the user specifies a COM port (e.g. "flash tracker to COM4"), use that port instead of COM3.
