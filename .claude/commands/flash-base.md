Compile and upload the Base Station firmware to the board.

1. Compile `Production/Base-Station` using:
   ```
   arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc Production/Base-Station
   ```
2. If compilation succeeds, upload to the COM port specified by the user (default: COM3):
   ```
   arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc -p COM3 Production/Base-Station
   ```
3. Report success or any errors. The serial monitor will start automatically after upload via the post-upload hook.

If the user specifies a COM port (e.g. "flash base to COM4"), use that port instead of COM3.
