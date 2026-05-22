#include "Arduino.h"
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include "index_html.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <Wire.h>

#define RF_FREQUENCY 915000000
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 10
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define BOARD_LED 35

struct GPSPacket {
  uint8_t status;  // [behavior(4)|fresh(1)|fix_valid(1)|ID(2)]
  uint8_t battery; // 0-100%
  float lat;
  float lon;
  uint16_t speed;  // mph * 100
  uint16_t course; // degrees * 100
};

struct WiFiNetwork {
  const char *ssid;
  const char *password;
};

WiFiNetwork networks[] = {
  {"RHIT-OPEN", ""},
  {"Test", "password"},
  {"ATO Wifi", "ATOest1865"}
};

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
static RadioEvents_t RadioEvents;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long ledFlashUntil = 0;

bool isRunActive = false;
String currentRunId = "";

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  for (auto &net : networks) {
    WiFi.begin(net.ssid, net.password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) delay(100);
      display.clear();
      display.drawString(0, 0, WiFi.localIP().toString());
      display.drawString(0, 13, "Listening...");
      display.display();
      return;
    }
    WiFi.disconnect();
    delay(200);
  }
}

void broadcastTelemetry(GPSPacket *pkt, int16_t rssi, int8_t snr) {
  StaticJsonDocument<300> doc;
  doc["type"] = "DATA";
  JsonObject p = doc.createNestedObject("payload");
  p["id"]    = pkt->status & 0x03;
  p["fix"]   = (pkt->status >> 2) & 1;
  p["fresh"] = (pkt->status >> 3) & 1;
  p["beh"]   = pkt->status >> 4;
  p["bat"]   = pkt->battery;
  p["lat"]   = pkt->lat;
  p["lng"]   = pkt->lon;
  p["spd"]   = pkt->speed / 100.0;
  p["hd"]    = pkt->course / 100.0;
  p["rssi"]  = rssi;
  p["snr"]   = snr;
  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  ledFlashUntil = millis() + 80;
  digitalWrite(BOARD_LED, HIGH);

  if (size == sizeof(GPSPacket)) {
    GPSPacket *pkt = (GPSPacket *)payload;
    broadcastTelemetry(pkt, rssi, snr);
  }

  Radio.Rx(0);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    StaticJsonDocument<128> doc;
    doc["type"]   = "STATE";
    doc["active"] = isRunActive;
    doc["run_id"] = currentRunId;
    String out;
    serializeJson(doc, out);
    client->text(out);
  }
}

void setup() {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  pinMode(BOARD_LED, OUTPUT);
  digitalWrite(BOARD_LED, LOW);

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Searching WiFi...");
  display.display();
  connectWiFi();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/api/run/start", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isRunActive) {
      isRunActive = true;
      currentRunId = request->hasParam("id") ? request->getParam("id")->value()
                                             : "run_" + String(millis());
      StaticJsonDocument<64> m;
      m["type"]   = "STATE";
      m["active"] = true;
      m["run_id"] = currentRunId;
      String out;
      serializeJson(m, out);
      ws.textAll(out);
      request->send(200, "application/json", "{\"status\":\"active\"}");
    } else {
      request->send(400, "text/plain", "Run already active");
    }
  });

  server.on("/api/run/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (isRunActive) {
      isRunActive = false;
      StaticJsonDocument<64> m;
      m["type"]   = "STATE";
      m["active"] = false;
      m["run_id"] = "";
      String out;
      serializeJson(m, out);
      ws.textAll(out);
      request->send(200, "application/json", "{\"status\":\"stopped\"}");
    } else {
      request->send(400, "text/plain", "No run active");
    }
  });

  server.begin();

  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0,
                    0, LORA_IQ_INVERSION_ON, true);
  Radio.Rx(0);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  ws.cleanupClients(2);

  if (ledFlashUntil && millis() > ledFlashUntil) {
    digitalWrite(BOARD_LED, LOW);
    ledFlashUntil = 0;
  }

  delay(1);

  Radio.IrqProcess();
}
