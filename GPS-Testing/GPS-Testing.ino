#include <TinyGPSPlus.h>

TinyGPSPlus gps;

#define PIN_GPS_EN 34
#define GPS_RX_PIN 39
#define GPS_TX_PIN 38
#define PIN_GPS_RESET 42
#define PIN_GPS_STANDBY 40

void setup() {
  Serial.begin(115200);

  pinMode(PIN_GPS_RESET, OUTPUT);
  digitalWrite(PIN_GPS_RESET, LOW);
  delay(200);
  digitalWrite(PIN_GPS_RESET, HIGH);

  pinMode(PIN_GPS_STANDBY, OUTPUT);
  digitalWrite(PIN_GPS_STANDBY, HIGH);

  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, LOW);
  delay(1000);

  Serial1.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS starting...");
}

void loop() {
  while (Serial1.available() > 0)
    gps.encode(Serial1.read());

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();

    if (gps.location.isValid()) {
      Serial.println(">> FIX ACQUIRED!");
      Serial.print("   ");
      Serial.print(gps.location.lat(), 6);
      Serial.print(", ");
      Serial.println(gps.location.lng(), 6);
      Serial.print("   SATS: ");
      Serial.println(gps.satellites.value());
    } else {
      Serial.print("No fix | Sats: ");
      Serial.print(gps.satellites.value());
      Serial.print(" | Chars: ");
      Serial.println(gps.charsProcessed());
    }
  }
}