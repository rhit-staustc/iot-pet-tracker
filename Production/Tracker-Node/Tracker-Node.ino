#include "Arduino.h"
#include "FastIMU.h"
#include "HT_TinyGPS++.h"
#include "LoRaWan_APP.h"
#include <Wire.h>

// #define DEBUG
#ifdef DEBUG
  #define DBG(x)   Serial.print(x)
  #define DBGLN(x) Serial.println(x)
#else
  #define DBG(x)
  #define DBGLN(x)
#endif

#define PIN_GPS_EN 34
#define GPS_RX_PIN 39
#define GPS_TX_PIN 38
#define PIN_GPS_RESET 42
#define AGE_THRESHOLD 5000
#define ADC_Ctrl 37
#define ADC_IN 1
#define MAX_MILLIV 4200
#define MIN_MILLIV 3500

#define RF_FREQUENCY 915000000
#define TX_OUTPUT_POWER 28
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 10
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE 30
#define BOARD_LED 35
#define TX_INTERVAL_MS 500

#define RUN_SPEED_IN_MPH 5
#define WALK_SPEED_IN_MPH 1
#define HEAD_DIRECTION accelData.accelX
#define HEAD_ORIENTATION HEAD_DIRECTION
#define SITTING_THRESHOLD 0.70
#define SPEED_FILTER_SIZE 8

uint8_t DEVICE_ID = 2;
const float VOLT_DIV = 4.9;

TinyGPSPlus gps;

MPU6500 IMU(Wire1);
calData calib = {0};
AccelData accelData;
GyroData gyroData;

float speedBuffer[SPEED_FILTER_SIZE] = {0};
uint8_t speedBufferIdx = 0;

enum BehaviorState : uint8_t {
  BEHAVIOR_IDLE    = 0,
  BEHAVIOR_WALKING = 1,
  BEHAVIOR_RUNNING = 2,
  BEHAVIOR_SITTING = 3,
  BEHAVIOR_ROLLING = 4,
};

// Pack behavior into the top 4 bits of the status byte: status |= (b << 4)

struct GPSPacket {
  uint8_t status;  // [behavior(4)|fresh(1)|fix_valid(1)|ID(2)]
  uint8_t battery; // 0-100%
  float lat;
  float lon;
  uint16_t speed;  // mph * 100
  uint16_t course; // degrees * 100
};

typedef enum { LOWPOWER, STATE_RX, STATE_TX } States_t;

static RadioEvents_t RadioEvents;
static unsigned long lastPacket = 0;
States_t state;
bool needsBusRecovery = false;

uint8_t getBatteryPercent() {
  pinMode(ADC_Ctrl, OUTPUT);
  digitalWrite(ADC_Ctrl, HIGH);
  uint32_t vBat = analogReadMilliVolts(ADC_IN) * VOLT_DIV;
  digitalWrite(ADC_Ctrl, LOW);
  int percent = ((int)vBat - MIN_MILLIV) * 100 / (MAX_MILLIV - MIN_MILLIV);
  return (uint8_t)constrain(percent, 0, 100);
}

double getFilteredSpeed() {
  speedBuffer[speedBufferIdx] = gps.speed.mph();
  speedBufferIdx = (speedBufferIdx + 1) % SPEED_FILTER_SIZE;

  double sum = 0;
  for (int i = 0; i < SPEED_FILTER_SIZE; i++) {
    sum += speedBuffer[i];
  }
  return sum / SPEED_FILTER_SIZE;
}

BehaviorState classifyBehavior(double speed_in_mph) {
  // behavior classification with gps sensor data
  if (speed_in_mph >= RUN_SPEED_IN_MPH) {
    return BEHAVIOR_RUNNING;
  } else if (speed_in_mph >= WALK_SPEED_IN_MPH) {
    return BEHAVIOR_WALKING;
  } else {
    // behavior subclassification with imu sensor data
    if (HEAD_ORIENTATION >= SITTING_THRESHOLD) {
      return BEHAVIOR_SITTING;
    } else {
      // TODO: roll-over detection not implemented
      return BEHAVIOR_IDLE;
    }
  }
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

  double filteredSpeed = getFilteredSpeed();
  packet.status |= (classifyBehavior(filteredSpeed) << 4);
  packet.battery = getBatteryPercent();
  packet.lat = (float)gps.location.lat();
  packet.lon = (float)gps.location.lng();
  packet.speed = (uint16_t)(filteredSpeed * 100);
  packet.course = (uint16_t)(gps.course.deg() * 100);
  return packet;
}

const char* behaviorName(uint8_t status) {
  switch ((BehaviorState)(status >> 4)) {
    case BEHAVIOR_WALKING: return "Walking";
    case BEHAVIOR_RUNNING: return "Running";
    case BEHAVIOR_SITTING: return "Sitting";
    case BEHAVIOR_ROLLING: return "Rolling";
    default:               return "Idle";
  }
}

void printPacket(GPSPacket &packet) {
  Serial.print("ID: ");
  Serial.print(packet.status & 0x03);
  Serial.print(" | Fix: ");
  Serial.print((packet.status >> 2) & 1 ? "YES" : "NO");
  Serial.print(" | Fresh: ");
  Serial.print((packet.status >> 3) & 1 ? "YES" : "NO");
  Serial.print(" | Behavior: ");
  Serial.print(behaviorName(packet.status));
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

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  Serial.println("Mcu initialized");

  Wire1.begin(6, 7);
  Wire1.setClock(400000);

  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, LOW);
  delay(1000);
  Serial1.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  gpsConfig();

  Serial.println("Initializing IMU...");
  IMU.init(calib, 0x68);
  Serial.println("IMU init complete. Keep IMU level.");
  delay(5000);
  Serial.println("Starting IMU calibration...");
  IMU.calibrateAccelGyro(&calib);
  IMU.init(calib, 0x68);  // re-init to apply biases and restore ±2000dps range
  Serial.println("Calibration done!");
  DBG("Accel biases X/Y/Z: ");
  DBG(calib.accelBias[0]); DBG(", ");
  DBG(calib.accelBias[1]); DBG(", ");
  DBGLN(calib.accelBias[2]);
  DBG("Gyro biases X/Y/Z: ");
  DBG(calib.gyroBias[0]); DBG(", ");
  DBG(calib.gyroBias[1]); DBG(", ");
  DBGLN(calib.gyroBias[2]);
  delay(2000);

  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON, true, 0,
                    0, LORA_IQ_INVERSION_ON, 3000);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0,
                    0, LORA_IQ_INVERSION_ON, true);

  state = STATE_TX;
}

void gpsConfig() {
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

void OnTxDone(void) {
  DBGLN("TX done");
  needsBusRecovery = true;
  state = STATE_RX;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  DBGLN("TX Timeout");
  needsBusRecovery = true;
  state = STATE_TX;
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  Radio.Sleep();
}

void loop() {
  switch (state) {
  case STATE_TX: {
    while (Serial1.available()) {
      gps.encode(Serial1.read());
    }
    if (needsBusRecovery) {
      Wire1.end();
      Wire1.begin(6, 7);
      Wire1.setClock(400000);
      IMU.init(calib, 0x68);
      needsBusRecovery = false;
    }
    IMU.update();
    IMU.getAccel(&accelData);
    IMU.getGyro(&gyroData);
    DBG("Accel X/Y/Z: "); DBG(accelData.accelX); DBG(", "); DBG(accelData.accelY); DBG(", "); DBGLN(accelData.accelZ);
    DBG("HeadOrient: "); DBG(HEAD_ORIENTATION); DBG(" (threshold: "); DBG(SITTING_THRESHOLD); DBGLN(")");
    GPSPacket packet = buildPacket();
    printPacket(packet);
    digitalWrite(BOARD_LED, HIGH);
    Radio.Send((uint8_t *)&packet, sizeof(GPSPacket));
    digitalWrite(BOARD_LED, LOW);
    lastPacket = millis();
    state = LOWPOWER;
    break;
  }
  case STATE_RX:
    DBGLN("into RX mode");
    Radio.Rx(0);
    lastPacket = millis();
    state = LOWPOWER;
    break;
  case LOWPOWER:
    while (Serial1.available()) {
      gps.encode(Serial1.read());
    }
    IMU.update();
    IMU.getAccel(&accelData);
    IMU.getGyro(&gyroData);
    Radio.IrqProcess();
    if (millis() - lastPacket >= TX_INTERVAL_MS) {
      Radio.Sleep();
      state = STATE_TX;
    }
    break;
  default:
    break;
  }
}