#include "HT_TinyGPS++.h"

TinyGPSPlus gps;

#define PIN_GPS_EN 34
#define GPS_RX_PIN 39
#define GPS_TX_PIN 38
#define PIN_GPS_RESET 42
#define PIN_GPS_STANDBY 40

#include <Wire.h> 
#include "HT_SSD1306Wire.h"

#ifdef WIRELESS_STICK_V3
static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED); // addr , freq , i2c group , resolution , rst
#else
static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst
#endif

void setup() {
  Serial.begin(115200);
  initDisplay();

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

void initDisplay(){
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);

  // Initialising the UI will init the display too.
  display.init();
  display.setFont(ArialMT_Plain_10);
}

void displayInfo() {
    // clear the display
    display.clear();

    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Sats: " + String(gps.satellites.value()));
    display.drawString(0, 16, "lat:  " + String(gps.location.lat(),8));
    display.drawString(0, 32, "lng: " + String(gps.location.lng(),8));
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 50, "Chars: " + String(gps.charsProcessed()));
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(10, 128, String(millis()));
  // write the buffer to the display
    display.display();
}


void loop() {
  while (Serial1.available() > 0)
    gps.encode(Serial1.read());

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    displayInfo();

    if (gps.location.isValid()) {
      Serial.println("location>> FIX ACQUIRED!");
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