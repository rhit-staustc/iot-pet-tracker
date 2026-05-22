#include "Arduino.h"
#include "FastIMU.h"
#include "HT_TinyGPS++.h"
#include "LoRaWan_APP.h"
#include <Wire.h>

#define SDA_PIN 45
#define SCL_PIN 46
#define PIN_GPS_EN 34
#define PIN_GPS_RESET 42
#define GPS_RX_PIN 39
#define GPS_TX_PIN 38
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
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BOARD_LED 35
#define TX_INTERVAL_MS 500

#define RUN_SPEED_IN_MPH     5
#define WALK_SPEED_IN_MPH    1
#define HEAD_ORIENTATION     accelData.accelZ
#define SITTING_THRESHOLD    0.70
#define ROLL_MIN_RATE        50.0
#define ROLL_ANGLE_THRESHOLD 270.0

uint8_t DEVICE_ID = 0; // 0 = Bryce, 1 = Ely
const float VOLT_DIV = 4.9;

TinyGPSPlus gps;

MPU6500 IMU(Wire1);
calData calib = {0};
AccelData accelData;
GyroData gyroData;

float rollAccum = 0;
unsigned long lastRollTime = 0;
bool rollingDetected = false;
unsigned long ledFlashUntil = 0;

enum BehaviorState : uint8_t {
  BEHAVIOR_IDLE    = 0,
  BEHAVIOR_WALKING = 1,
  BEHAVIOR_RUNNING = 2,
  BEHAVIOR_SITTING = 3,
  BEHAVIOR_ROLLING = 4,
};

struct GPSPacket {
  uint8_t status;  // [behavior(4)|fresh(1)|fix_valid(1)|ID(2)]
  uint8_t battery; // 0-100%
  float lat;
  float lon;
  uint16_t speed;  // mph * 100
  uint16_t course; // degrees * 100
};

typedef enum { LOWPOWER, STATE_TX } States_t;

static RadioEvents_t RadioEvents;
static unsigned long lastPacket = 0;
States_t state;
bool needsBusRecovery = false;

uint8_t getBatteryPercent() {
  digitalWrite(ADC_Ctrl, HIGH);
  uint32_t vBat = analogReadMilliVolts(ADC_IN) * VOLT_DIV;
  digitalWrite(ADC_Ctrl, LOW);
  int percent = ((int)vBat - MIN_MILLIV) * 100 / (MAX_MILLIV - MIN_MILLIV);
  return (uint8_t)constrain(percent, 0, 100);
}

void updateRolling() {
  unsigned long now = millis();
  float dt = (now - lastRollTime) / 1000.0f;
  lastRollTime = now;
  float gyroMag = sqrt(sq(gyroData.gyroX) + sq(gyroData.gyroY) + sq(gyroData.gyroZ));
  if (gyroMag >= ROLL_MIN_RATE) {
    rollAccum += gyroMag * dt;
  } else {
    rollAccum = 0;
  }
  if (rollAccum >= ROLL_ANGLE_THRESHOLD) {
    rollingDetected = true;
    rollAccum = 0;
  }
}

BehaviorState classifyBehavior(double speed_in_mph) {
  if (speed_in_mph >= RUN_SPEED_IN_MPH) {
    return BEHAVIOR_RUNNING;
  } else if (speed_in_mph >= WALK_SPEED_IN_MPH) {
    return BEHAVIOR_WALKING;
  } else {
    if (rollingDetected) {
      rollingDetected = false;
      return BEHAVIOR_ROLLING;
    } else if (HEAD_ORIENTATION <= SITTING_THRESHOLD && HEAD_ORIENTATION > 0) {
      return BEHAVIOR_SITTING;
    } else {
      return BEHAVIOR_IDLE;
    }
  }
}

GPSPacket buildPacket() {
  GPSPacket packet;
  bool fixValid = gps.location.isValid();
  bool fresh = gps.location.age() < AGE_THRESHOLD;
  packet.status = (DEVICE_ID & 0x03);
  if (fixValid) packet.status |= (1 << 2);
  if (fresh)    packet.status |= (1 << 3);
  double speed = gps.speed.mph();
  packet.status |= (classifyBehavior(speed) << 4);
  packet.battery = getBatteryPercent();
  packet.lat = (float)gps.location.lat();
  packet.lon = (float)gps.location.lng();
  packet.speed = (uint16_t)(speed * 100);
  packet.course = (uint16_t)(gps.course.deg() * 100);
  return packet;
}

void OnTxDone(void) {
  ledFlashUntil = millis() + 80;
  digitalWrite(BOARD_LED, HIGH);
  needsBusRecovery = true;
  state = LOWPOWER;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  needsBusRecovery = true;
  state = STATE_TX;
}

void gpsConfig() {
  Serial1.println("$PCAS04,7*1E");
  Serial1.println("$PCAS11,1*1C");
  Serial1.println("$PCAS02,1000*2E");
  Serial1.println("$PCAS03,0,0,0,0,1,0,0,0,0,0,,,0,0*03");
  Serial1.println("$PCAS00*01");
}

void setup() {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  Wire1.begin(SDA_PIN, SCL_PIN);
  Wire1.setClock(400000);

  pinMode(PIN_GPS_RESET, OUTPUT);
  digitalWrite(PIN_GPS_RESET, HIGH);

  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, LOW);
  delay(1000);
  Serial1.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  gpsConfig();

  calib.valid = true;
  calib.accelBias[0] =  0.02;
  calib.accelBias[1] =  0.03;
  calib.accelBias[2] = -0.01;
  calib.gyroBias[0]  =  3.06;
  calib.gyroBias[1]  =  0.45;
  calib.gyroBias[2]  = -0.45;
  IMU.init(calib, 0x68);

  pinMode(ADC_Ctrl, OUTPUT);
  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON, true, 0,
                    0, LORA_IQ_INVERSION_ON, 3000);

  state = STATE_TX;
}

void loop() {
  switch (state) {
  case STATE_TX: {
    while (Serial1.available()) gps.encode(Serial1.read());
    if (needsBusRecovery) {
      Wire1.end();
      Wire1.begin(SDA_PIN, SCL_PIN);
      Wire1.setClock(400000);
      needsBusRecovery = false;
    }
    IMU.update();
    IMU.getAccel(&accelData);
    IMU.getGyro(&gyroData);
    updateRolling();
    GPSPacket packet = buildPacket();
    Radio.Send((uint8_t *)&packet, sizeof(GPSPacket));
    lastPacket = millis();
    state = LOWPOWER;
    break;
  }
  case LOWPOWER:
    while (Serial1.available()) gps.encode(Serial1.read());
    IMU.update();
    IMU.getAccel(&accelData);
    IMU.getGyro(&gyroData);
    updateRolling();
    if (ledFlashUntil && millis() > ledFlashUntil) {
      digitalWrite(BOARD_LED, LOW);
      ledFlashUntil = 0;
    }
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
