#include "Arduino.h"
#include "LoRaWan_APP.h"

// --- DISPLAY: includes ---
#include "HT_SSD1306Wire.h"
#include <Wire.h>

// --- DISPLAY: init ---
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64,
                           RST_OLED);

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

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

typedef enum { LOWPOWER, STATE_RX, STATE_TX } States_t;

int16_t txNumber;
States_t state;
bool sleepMode = false;
int16_t Rssi, rxSize;

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  txNumber = 0;
  Rssi = 0;

  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

  // --- DISPLAY: power on and init ---
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

void loop() {
  switch (state) {
  case STATE_TX:
    txNumber++;
    sprintf(txpacket, "hello %d, Rssi : %d", txNumber, Rssi);
    Serial.printf("\r\nsending packet \"%s\" , length %d\r\n", txpacket,
                  strlen(txpacket));
    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    state = LOWPOWER;
    break;
  case STATE_RX:
    Serial.println("into RX mode");
    Radio.Rx(0);
    state = LOWPOWER;
    break;
  case LOWPOWER:
    Radio.IrqProcess();
    break;
  default:
    break;
  }
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
  digitalWrite(BOARD_LED, HIGH);
  delay(50);
  digitalWrite(BOARD_LED, LOW);

  Rssi = rssi;
  rxSize = size;
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Radio.Sleep();

  Serial.printf("\r\nreceived packet \"%s\" with Rssi %d , length %d\r\n",
                rxpacket, Rssi, rxSize);
  Serial.println("wait to send next packet");

  // --- DISPLAY: parse RSSI from packet string and show both ---
  int packetNum, msgRssi;
  sscanf(rxpacket, "hello %d, Rssi : %d", &packetNum, &msgRssi);

  // --- DISPLAY: show radio RSSI and the RSSI value inside the packet ---
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Radio RSSI: " + String(rssi) + " dBm");
  display.drawString(0, 16, "Msg RSSI:   " + String(msgRssi) + " dBm");
  display.display();

  state = STATE_TX;
}