#include "Arduino.h"
#include "LoRaWan_APP.h"
#include "index_html.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

// LoRa Config
#define RF_FREQUENCY 915000000
#define TX_OUTPUT_POWER 28
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 10
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

struct WiFiNetwork {
  const char *ssid;
  const char *password;
};

WiFiNetwork networks[] = {
  {"RHIT-OPEN", ""}
};

// WiFiNetwork networks[] = {
//   {"RHIT-OPEN", ""},
//   {"Test", "password"},
//   {"ATO Wifi", "ATOest1865"}
// };

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// System State
bool isRunActive = false;
String currentRunId = "";

// Packets struct
struct GPSPacket {
  uint8_t status;  // [behavior(4)|fresh(1)|fix_valid(1)|ID(2)]
  uint8_t battery; // 0-100%
  float lat;
  float lon;
  uint16_t speed;  // mph * 100
  uint16_t course; // degrees * 100
};

static RadioEvents_t RadioEvents;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  for (auto &net : networks) {
    Serial.printf("Trying %s...\n", net.ssid);
    WiFi.begin(net.ssid, net.password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Connected to %s\n", net.ssid);
      Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
      return;
    }
    WiFi.disconnect();
    delay(200);
  }
  Serial.println("All networks failed.");
}

// Telemetry Core — broadcast only, browser handles storage
void handleTrackerPayload(GPSPacket *pkt, int16_t rssi, int8_t snr) {
  uint8_t trackerId = pkt->status & 0x03;
  bool fixValid = (pkt->status >> 2) & 1;
  uint8_t behavior = pkt->status >> 4;

  StaticJsonDocument<300> jsonDoc;
  jsonDoc["type"] = "DATA";
  JsonObject p = jsonDoc.createNestedObject("payload");
  p["id"] = trackerId;
  p["fix"] = fixValid;
  p["beh"] = behavior;
  p["bat"] = pkt->battery;
  p["lat"] = pkt->lat;
  p["lng"] = pkt->lon;
  p["spd"] = pkt->speed / 100.0;
  p["hd"] = pkt->course / 100.0;
  p["rssi"] = rssi;
  p["snr"] = snr;

  String wsOutput;
  serializeJson(jsonDoc, wsOutput);
  ws.textAll(wsOutput);
}

// LoRa Intercept Handling
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  if (size == sizeof(GPSPacket))
    handleTrackerPayload((GPSPacket *)payload, rssi, snr);
  Radio.Rx(0);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    StaticJsonDocument<128> stateDoc;
    stateDoc["type"] = "STATE";
    stateDoc["active"] = isRunActive;
    stateDoc["run_id"] = currentRunId;
    String out;
    serializeJson(stateDoc, out);
    client->text(out);
  }
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  if (!LittleFS.begin(true))
    Serial.println("LittleFS Mounting Error!");

  connectWiFi();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/api/run/start", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isRunActive) {
      isRunActive = true;
      currentRunId = "run_" + String(millis());
      StaticJsonDocument<64> m;
      m["type"] = "STATE";
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
      m["type"] = "STATE";
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

  // Browser POSTs the full track here when run stops
  server.on(
      "/api/run/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        String id = request->hasParam("id") ? request->getParam("id")->value()
                                            : "run_" + String(millis());
        File f = LittleFS.open("/" + id + ".json", "w");
        if (f) {
          f.write(data, len);
          f.close();
          request->send(200, "text/plain", "Saved");
        } else {
          request->send(500, "text/plain", "Write failed");
        }
      });

  server.on("/api/results", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
      String name = String(file.name());
      if (name.endsWith(".json"))
        arr.add(name.substring(0, name.length() - 5)); // fixed off-by-one
      file = root.openNextFile();
    }
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  server.on("/api/results/view", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id"))
      request->send(LittleFS, "/" + request->getParam("id")->value() + ".json",
                    "application/json");
    else
      request->send(400, "text/plain", "Missing id");
  });

  server.on(
      "/api/packet", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
         size_t index, size_t total) {
        if (len == sizeof(GPSPacket)) {
          handleTrackerPayload((GPSPacket *)data, -50, 10);
          request->send(200, "text/plain", "Acknowledged");
        } else {
          request->send(400, "text/plain", "Size Mismatch");
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
  if (WiFi.status() != WL_CONNECTED)
    connectWiFi();
  Radio.IrqProcess();
  ws.cleanupClients();
  delay(1);
}