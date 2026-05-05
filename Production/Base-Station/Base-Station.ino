#include "Arduino.h"
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include <Wire.h>

#define RF_FREQUENCY 900000000
#define TX_OUTPUT_POWER 28
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BOARD_LED 35
#define MAX_TRACKERS 4
#define RX_WINDOW_MS 500

struct GPSPacket {
  uint8_t status;  // [reserved(4)|fresh(1)|fix_valid(1)|ID(2)]
  uint8_t battery; // 0-100%
  float lat;
  float lon;
  uint16_t speed;  // mph * 100
  uint16_t course; // degrees * 100
};

struct CmdPacket {
  uint8_t target_id; // 0-3 or 0xFF for all
  uint8_t command;   // tbd
};

struct TrackerState {
  GPSPacket last;
  int16_t rssi;
  bool active;
};

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64,
                           RST_OLED);
static RadioEvents_t RadioEvents;

TrackerState trackers[MAX_TRACKERS] = {};
uint8_t displayedTracker = 0;

bool cmdPending = false;
CmdPacket pendingCmd;

typedef enum { LOWPOWER, STATE_RX, STATE_TX } States_t;
States_t state;
static unsigned long lastStateChange = 0;

void OnTxDone(void) {
  Serial.println("CMD sent.");
  state = STATE_RX;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("TX Timeout.");
  state = STATE_RX;
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  digitalWrite(BOARD_LED, HIGH);
  delay(20);
  digitalWrite(BOARD_LED, LOW);

  if (size == sizeof(GPSPacket)) {
    GPSPacket *pkt = (GPSPacket *)payload;
    uint8_t id = pkt->status & 0x03;
    if (id < MAX_TRACKERS) {
      trackers[id].last = *pkt;
      trackers[id].rssi = rssi;
      trackers[id].active = true;

      Serial.printf("[RX] ID:%d Fix:%s Fresh:%s Bat:%d%% Lat:%.6f Lon:%.6f "
                    "Spd:%.1f Dir:%.1f RSSI:%d\n",
                    id, (pkt->status >> 2) & 1 ? "Y" : "N",
                    (pkt->status >> 3) & 1 ? "Y" : "N", pkt->battery, pkt->lat,
                    pkt->lon, pkt->speed / 100.0, pkt->course / 100.0, rssi);

      updateDisplay(id);
    }
  }

  Radio.Sleep();
  state = STATE_RX;
}

void updateDisplay(uint8_t id) {
  displayedTracker = id;
  TrackerState &t = trackers[id];

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(
      0, 0,
      "ID:" + String(id) +
          " Fix:" + String((t.last.status >> 2) & 1 ? "Y" : "N") +
          " Fresh:" + String((t.last.status >> 3) & 1 ? "Y" : "N") +
          " Bat:" + String(t.last.battery) + "%");
  display.drawString(0, 13, "Latitude: " + String(t.last.lat, 5));
  display.drawString(0, 26, "Longitude: " + String(t.last.lon, 5));
  display.drawString(0, 39,
                     "Speed: " + String(t.last.speed / 100.0, 1) +
                         "mph Dir: " + String(t.last.course / 100.0, 1));
  display.drawString(0, 52, "RSSI: " + String(t.rssi) + " dBm");
  display.display();
}

void handleSerialCommand() {
  if (!Serial.available())
    return;
  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line.startsWith("reset ")) {
    String arg = line.substring(6);
    arg.trim();
    pendingCmd.command = 0x01;
    pendingCmd.target_id = (arg == "all") ? 0xFF : (uint8_t)arg.toInt();
    cmdPending = true;
    Serial.printf("Queued GPS reset -> target %s\n", arg.c_str());

  } else if (line.startsWith("show ")) {
    uint8_t id = (uint8_t)line.substring(5).toInt();
    if (id < MAX_TRACKERS && trackers[id].active) {
      updateDisplay(id);
      Serial.printf("Showing tracker %d\n", id);
    } else {
      Serial.printf("Tracker %d not active\n", id);
    }

  } else if (line.length() > 0) {
    Serial.println("Commands: reset <id|all> | show <id>");
  }
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Base station ready");
  display.drawString(0, 12, "Listening...");
  display.display();

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

  state = STATE_RX;
}

void loop() {
  handleSerialCommand();

  switch (state) {
  case STATE_RX:
    Radio.Rx(0);
    lastStateChange = millis();
    state = LOWPOWER;
    break;

  case STATE_TX: {
    CmdPacket cmd = pendingCmd;
    cmdPending = false;
    Radio.Send((uint8_t *)&cmd, sizeof(CmdPacket));
    lastStateChange = millis();
    state = LOWPOWER;
    break;
  }

  case LOWPOWER:
    Radio.IrqProcess();
    if (cmdPending && (millis() - lastStateChange >= RX_WINDOW_MS)) {
      Radio.Sleep();
      state = STATE_TX;
    }
    break;

  default:
    break;
  }
}
