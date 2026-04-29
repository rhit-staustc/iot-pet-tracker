#include "HT_SSD1306Wire.h"
#include "HT_TinyGPS++.h"
// #include <Wire.h>

#define PIN_GPS_EN 34
#define GPS_RX_PIN 39
#define GPS_TX_PIN 38
#define PIN_GPS_RESET 42
#define PIN_GPS_STANDBY 40

TinyGPSPlus gps;
String gpsResponse = "";
String gnssMode = "UNKNOWN";

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64,
                           RST_OLED);

void setup() {
  Serial.begin(115200);
  initDisplay();
  // pinMode(PIN_GPS_RESET, OUTPUT);
  // digitalWrite(PIN_GPS_RESET, LOW);
  // delay(200);
  // digitalWrite(PIN_GPS_RESET, HIGH);
  // pinMode(PIN_GPS_STANDBY, OUTPUT);
  // digitalWrite(PIN_GPS_STANDBY, HIGH);
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, LOW);
  delay(1000);

  Serial1.begin(115200, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("GPS starting...");
  sendPCAS("$PCAS04,7");                         // all constellations
  sendPCAS("$PCAS11,1");                         // pedestrian mode
  sendPCAS("$PCAS02,1000");                      // 1hz
  sendPCAS("$PCAS03,1,0,0,0,1,0,0,0,0,0,,,0,0"); // min NMEA
  sendPCAS("$PCAS00");                           // save to flash
  Serial.println("GPS configured and saved!");
}

void initDisplay() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);
  display.init();
  display.setFont(ArialMT_Plain_10);
}

void displayInfo() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Sats: " + String(gps.satellites.value()));
  display.drawString(0, 16, "lat:  " + String(gps.location.lat(), 6));
  display.drawString(0, 32, "lng: " + String(gps.location.lng(), 6));
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 48, "HDOP: " + String(gps.hdop.hdop()));
  display.drawString(64, 48, "Fixes: " + String(gps.sentencesWithFix()));
  display.display();
}

void sendPCAS(String cmd) {
  uint8_t checksum = 0;
  for (int i = 1; i < cmd.length(); i++) {
    checksum ^= cmd[i];
  }
  char cs[3];
  sprintf(cs, "%02X", checksum);
  String full = cmd + "*" + cs;
  Serial1.println(full);
  Serial.println("Sent: " + full);
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd == "1hz")
    sendPCAS("$PCAS02,1000"); // 1Hz update rate
  else if (cmd == "2hz")
    sendPCAS("$PCAS02,500"); // 2Hz update rate
  else if (cmd == "5hz")
    sendPCAS("$PCAS02,200"); // 5Hz update rate
  else if (cmd == "10hz")
    sendPCAS("$PCAS02,100"); // 10Hz update rate
  else if (cmd == "gps")
    sendPCAS("$PCAS04,1"); // GPS only
  else if (cmd == "bds")
    sendPCAS("$PCAS04,2"); // BeiDou only
  else if (cmd == "glo")
    sendPCAS("$PCAS04,4"); // GLONASS only
  else if (cmd == "gpsglo")
    sendPCAS("$PCAS04,5"); // GPS + GLONASS
  else if (cmd == "all")
    sendPCAS("$PCAS04,7"); // GPS + BeiDou + GLONASS
  else if (cmd == "hot")
    sendPCAS("$PCAS10,0"); // hot start - fastest, uses all stored data
  else if (cmd == "warm")
    sendPCAS("$PCAS10,1"); // warm start - keeps almanac
  else if (cmd == "cold")
    sendPCAS("$PCAS10,2"); // cold start - full reacquire
  else if (cmd == "fcold")
    sendPCAS("$PCAS10,3"); // factory reset - wipes everything
  else if (cmd == "save")
    sendPCAS("$PCAS00"); // save config to flash
  else if (cmd == "version")
    sendPCAS("$PCAS06,0"); // query firmware version
  else if (cmd == "min")
    sendPCAS("$PCAS03,1,0,0,0,1,0,0,0,0,0,,,0,0"); // GGA+RMC only
  else if (cmd == "full")
    sendPCAS("$PCAS03,1,1,1,1,1,1,1,1,0,0,,,0,0"); // all sentences
  else if (cmd == "rmc")
    sendPCAS("$PCAS03,0,0,0,0,1,0,0,0,0,0,,,0,0"); // rmc
  else if (cmd == "cga")
    sendPCAS("$PCAS03,1,0,0,0,0,0,0,0,0,0,,,0,0"); // cga
  else
    Serial.println(
        "Unknown command. Options: 1hz/2hz/5hz/10hz, gps/bds/glo/gpsglo/all, "
        "hot/warm/cold/fcold, save, version, min/full");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
  }

  while (Serial1.available() > 0) {
    char c = Serial1.read();
    gps.encode(c);
    gpsResponse += c;
    if (c == '\n') {
      if (gpsResponse.startsWith("$GN"))
        gnssMode = "MULTI";
      else if (gpsResponse.startsWith("$GP"))
        gnssMode = "GPS";
      else if (gpsResponse.startsWith("$GL"))
        gnssMode = "GLO";
      else if (gpsResponse.startsWith("$BD"))
        gnssMode = "BDS";

      if (gpsResponse.startsWith("$GPTXT") &&
          gpsResponse.indexOf(",02,") != -1) {
        Serial.print("GPS: " + gpsResponse);
      }
      gpsResponse = "";
    }
    if (gpsResponse.length() > 100) {
      gpsResponse = "";
    }
  }

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    displayInfo();
    Serial.print("Located?: ");
    Serial.print(gps.location.isValid() ? "YES" : "NO");
    Serial.print(" | Sats: ");
    Serial.print(gps.satellites.value());
    Serial.print(" | HDOP: ");
    Serial.print(gps.hdop.hdop());
    Serial.print(" | Fixes: ");
    Serial.print(gps.sentencesWithFix());
    Serial.print(" | Bad: ");
    Serial.print(gps.failedChecksum());
    Serial.print(" | Mode: ");
    Serial.print(gnssMode);
    Serial.print(" | Speed: ");
    Serial.print(gps.speed.mph());
    Serial.print("mph | Dir: ");
    Serial.print(gps.cardinal(gps.course.deg()));
    Serial.print(" | FixQ: ");
    Serial.println((int)gps.location.FixQuality() - 48);
  }
}