#include "HT_TinyGPS++.h"

#define PIN_GPS_EN 34
#define GPS_RX_PIN 39
#define GPS_TX_PIN 38
#define PIN_GPS_RESET 42
#define AGE_THRESHOLD 5000
#define ADC_Ctrl 37
#define ADC_IN 1
#define MAX_MILLIV 4200
#define MIN_MILLIV 3500

const float VOLT_DIV = 4.9;
uint8_t DEVICE_ID = 0;
TinyGPSPlus gps;

struct GPSPacket {
  uint8_t status;  // [reserved(4)|fresh(1)|fix_valid(1)|HID(2)]
  uint8_t battery; // 0-100%
  float lat;
  float lon;
  uint16_t speed;  // mph * 100
  uint16_t course; // degrees * 100
};

uint8_t getBatteryPercent() {
  pinMode(ADC_Ctrl, OUTPUT);
  digitalWrite(ADC_Ctrl, HIGH);
  uint32_t vBat = analogReadMilliVolts(ADC_IN) * VOLT_DIV;
  digitalWrite(ADC_Ctrl, LOW);
  int percent = ((int)vBat - MIN_MILLIV) * 100 / (MAX_MILLIV - MIN_MILLIV);
  return (uint8_t)constrain(percent, 0, 100);
}

GPSPacket buildPacket() {
  GPSPacket packet;
  bool fixValid = gps.location.isValid();
  bool fresh = gps.location.age() < AGE_THRESHOLD;
  packet.status = (DEVICE_ID & 0x03);
  if (fixValid)
    packet.status |= (1 << 2);
  if (fresh)
    packet.status |= (1 << 3);
  packet.battery = getBatteryPercent();
  packet.lat = (float)gps.location.lat();
  packet.lon = (float)gps.location.lng();
  packet.speed = (uint16_t)(gps.speed.mph() * 100);
  packet.course = (uint16_t)(gps.course.deg() * 100);
  return packet;
}

void printPacket(GPSPacket &packet) {
  Serial.print("ID: ");
  Serial.print(packet.status & 0x03);
  Serial.print(" | Fix: ");
  Serial.print((packet.status >> 2) & 1 ? "YES" : "NO");
  Serial.print(" | Fresh: ");
  Serial.print((packet.status >> 3) & 1 ? "YES" : "NO");
  Serial.print(" | Batt: ");
  Serial.print(packet.battery);
  Serial.print("% | Lat: ");
  Serial.print(packet.lat, 6);
  Serial.print(" | Lon: ");
  Serial.print(packet.lon, 6);
  Serial.print(" | Speed: ");
  Serial.print(packet.speed / 100.0);
  Serial.print("mph | Dir: ");
  Serial.println(packet.course / 100.0);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, LOW);
  delay(1000);
  Serial1.begin(115200, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void config() {
  Serial.println("Enter device ID (0-3):");
  while (!Serial.available());
  DEVICE_ID = Serial.readStringUntil('\n').toInt();
  DEVICE_ID = constrain(DEVICE_ID, 0, 3);
  Serial.print("Device ID: ");
  Serial.println(DEVICE_ID);
  Serial.println("GPS starting...");
  Serial1.println("$PCAS04,7*1E");                         // all constellations
  Serial1.println("$PCAS11,1*1C");                         // pedestrian mode
  Serial1.println("$PCAS02,1000*2E");                      // 1hz
  Serial1.println("$PCAS03,0,0,0,0,1,0,0,0,0,0,,,0,0*03"); // RMC mode
  Serial1.println("$PCAS00*01");                           // save to flash
  Serial.println("GPS configured and saved!");
}

void loop() {
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    if (cmd == "config") {
      config();
    }
  }

  static unsigned long lastPacket = 0;
  if (millis() - lastPacket > 1000) {
    lastPacket = millis();
    GPSPacket packet = buildPacket();
    printPacket(packet);
    // TODO: send over LoRa
  }
}
