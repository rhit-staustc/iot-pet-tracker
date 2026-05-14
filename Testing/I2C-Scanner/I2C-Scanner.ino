#include <Wire.h>
// WARNING: Uses Wire (not Wire1). Flash and run this sketch standalone, before any sketch that calls Mcu.begin(),
// which reconfigures Wire to the OLED pins (17/18) and breaks I2C access to the IMU on pins 6/7.

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);
  Wire.setClock(400000);

  Serial.println("\nI2C Scanner");
  Serial.println("Scanning for devices on pins 6 (SDA) and 7 (SCL)...\n");

  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("Scan complete\n");
}

void loop() {
  delay(5000);
  Serial.println("Rescanning...");

  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" ");
      nDevices++;
    }
  }
  Serial.println();
}
