#include "Arduino.h"
#include "HT_SSD1306Wire.h"
#include "HT_TinyGPS++.h"
#include "LoRaWan_APP.h"
#include <Wire.h>

#define PIN_GPS_EN 34
#define GPS_RX_PIN 39
#define GPS_TX_PIN 38
#define PIN_GPS_RESET 42
#define AGE_THRESHOLD 5000
#define ADC_Ctrl 37
#define ADC_IN 1
#define MAX_MILLIV 4200
#define MIN_MILLIV 3500

#define RF_FREQUENCY 900000000
#define TX_OUTPUT_POWER 28
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 1000
#define BUFFER_SIZE 30
#define BOARD_LED 35
#define TX_INTERVAL_MS 500

TinyGPSPlus gps;

const float VOLT_DIV = 4.9;
uint8_t DEVICE_ID = 2;

struct GPSPacket {
  uint8_t status;  // [reserved(4)|fresh(1)|fix_valid(1)|ID(2)]
  uint8_t battery; // 0-100%
  float lat;
  float lon;
  uint16_t speed;  // mph * 100
  uint16_t course; // degrees * 100
};

typedef enum { LOWPOWER, STATE_RX, STATE_TX } States_t;

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64,
                           RST_OLED);
static RadioEvents_t RadioEvents;
static unsigned long lastPacket = 0;
States_t state;
bool sleepMode = false;

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
  Serial1.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  gpsConfig();

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);
  display.init();
  display.setFont(ArialMT_Plain_16);

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
  Serial.print("TX done......");
  state = STATE_RX;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.print("TX Timeout......");
  state = STATE_TX;
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  Radio.Sleep();

  if (size == sizeof(GPSPacket)) {
    GPSPacket *received = (GPSPacket *)payload;
  }

  state = STATE_TX;
}

void updateDisplay(GPSPacket &packet) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(
      0, 0,
      "ID:" + String(packet.status & 0x03) +
          " Fix:" + String((packet.status >> 2) & 1 ? "Y" : "N") +
          " Fresh:" + String((packet.status >> 3) & 1 ? "Y" : "N") +
          " Bat:" + String(packet.battery) + "%");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 13, "Lat: " + String(packet.lat, 5));
  display.drawString(0, 32, "Lon: " + String(packet.lon, 5));
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 51,
                     "Spd: " + String(packet.speed / 100.0, 1) + "mph" +
                         "  Dir: " + String(packet.course / 100.0, 1));
  display.display();
}

void loop() {
  switch (state) {
  case STATE_TX: {
    while (Serial1.available()) {
      gps.encode(Serial1.read());
    }
    GPSPacket packet = buildPacket();
    printPacket(packet);
    updateDisplay(packet);
    digitalWrite(BOARD_LED, HIGH);
    Radio.Send((uint8_t *)&packet, sizeof(GPSPacket));
    digitalWrite(BOARD_LED, LOW);
    lastPacket = millis();
    state = LOWPOWER;
    break;
  }
  case STATE_RX:
    Serial.println("into RX mode");
    Radio.Rx(0);
    lastPacket = millis();
    state = LOWPOWER;
    break;
  case LOWPOWER:
    while (Serial1.available()) {
      gps.encode(Serial1.read());
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