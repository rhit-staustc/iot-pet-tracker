#include "Arduino.h"
#include "LoRaWan_APP.h"

#define RF_FREQUENCY 915000000
#define TX_OUTPUT_POWER 28
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 12
#define LORA_CODINGRATE 4
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BUFFER_SIZE 30
#define PRG_BUTTON 0
#define LED_PIN 35

enum TxState { IDLE, SENDING };

char txpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;
int16_t txNumber = 0;
TxState txState = IDLE;
bool transmitting = false;
bool lastButton = HIGH;

void OnTxDone() {
  txState = IDLE;
  Serial.println("TX done");
}

void OnTxTimeout() {
  Radio.Sleep();
  txState = IDLE;
  Serial.println("TX timeout");
}

void startTransmitting() {
  transmitting = true;
  txState = IDLE;
  digitalWrite(LED_PIN, HIGH);
  Serial.println("Transmitting: ON");
}

void stopTransmitting() {
  transmitting = false;
  txState = IDLE;
  Radio.Sleep();
  digitalWrite(LED_PIN, LOW);
  Serial.println("Transmitting: OFF");
}

void sendPacket() {
  txState = SENDING;
  sprintf(txpacket, "Hello RX #%d", txNumber++);
  Serial.printf("Sending: \"%s\"\n", txpacket);
  Radio.Send((uint8_t *)txpacket, strlen(txpacket));
}

void setup() {
  Serial.begin(115200);
  pinMode(PRG_BUTTON, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON, true, 0,
                    0, LORA_IQ_INVERSION_ON, 3000);

  Serial.println("Press PRG to toggle transmission");
}

void loop() {
  bool currentButton = digitalRead(PRG_BUTTON);
  if (lastButton == HIGH && currentButton == LOW) {
    delay(50);
    transmitting ? stopTransmitting() : startTransmitting();
  }
  lastButton = currentButton;

  if (transmitting && txState == IDLE) {
    sendPacket();
  }

  Radio.IrqProcess();
}