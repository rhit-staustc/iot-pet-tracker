#include "FastIMU.h"
#include "HT_SSD1306Wire.h"
#include "HT_TinyGPS++.h"
#include <Wire.h>

TinyGPSPlus gps;

MPU6500 IMU;
calData calib = {0};
AccelData accelData;
GyroData gyroData;

// behavior classification constants
#define RUN_SPEED_IN_MPH 5 
#define WALK_SPEED_IN_MPH 1
#define HEAD_DIRECTION gyroData.gyroX

// pin definitions
#define PIN_GPS_EN 34
#define GPS_RX_PIN 39
#define GPS_TX_PIN 38
#define PIN_GPS_RESET 42
#define PIN_GPS_STANDBY 40

#ifdef WIRELESS_STICK_V3
static SSD1306Wire
    display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32,
            RST_OLED); // addr , freq , i2c group , resolution , rst
#else
static SSD1306Wire
    display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64,
            RST_OLED); // addr , freq , i2c group , resolution , rst
#endif

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);
  Wire.setClock(400000);
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

  IMU.init(calib, 0x68);

  Serial.println("Keep IMU level.");
  delay(5000);
  IMU.calibrateAccelGyro(&calib);
  Serial.println("Calibration done!");
  Serial.print("Accel biases X/Y/Z: ");
  Serial.print(calib.accelBias[0]);
  Serial.print(", ");
  Serial.print(calib.accelBias[1]);
  Serial.print(", ");
  Serial.println(calib.accelBias[2]);
  Serial.print("Gyro biases X/Y/Z: ");
  Serial.print(calib.gyroBias[0]);
  Serial.print(", ");
  Serial.print(calib.gyroBias[1]);
  Serial.print(", ");
  Serial.println(calib.gyroBias[2]);
  delay(2000);
}

void initDisplay() {
  pinMode(Vext, OUTPUT);
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
  display.drawString(0, 16, "lat:  " + String(gps.location.lat(), 8));
  display.drawString(0, 32, "lng: " + String(gps.location.lng(), 8));
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
  IMU.update();
  IMU.getAccel(&accelData);
  IMU.getGyro(&gyroData);
  // Serial.print(accelData.accelX);
  // Serial.print("\t");
  // Serial.print(accelData.accelY);
  // Serial.print("\t");
  // Serial.print(accelData.accelZ);
  // Serial.print("\t");
  // Serial.print(gyroData.gyroX);
  // Serial.print("\t");
  // Serial.print(gyroData.gyroY);
  // Serial.print("\t");
  // Serial.println(gyroData.gyroZ);
  
  double speed_in_mph = gps.speed.mph();
  // behavior classification with gps sensor data
  if (speed_in_mph >= RUN_SPEED_IN_MPH) {
    Serial.println("Running");
  } else if (speed_in_mph >= WALK_SPEED_IN_MPH) {
    Serial.println("Walking");
  } else {
    Serial.println("Idle");

    // behavior subclassification with imu sensor data
  }
  
  
  delay(50);
}