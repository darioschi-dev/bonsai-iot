#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <PubSubClient.h>
#include "config.h"

extern AsyncWebServer server;
extern Config config;
extern PubSubClient mqttClient;

void saveConfigFromJson(const String& jsonString) {
  File file = SPIFFS.open("/config.json", "w");
  if (!file) {
    Serial.println("[❌] Impossibile aprire config.json in scrittura");
    return;
  }
  file.print(jsonString);
  file.close();
  Serial.println("[✅] Config salvato, riavvio...");
  delay(1000);
  ESP.restart();
}

void setupConfigApi() {
  // HTTP GET /api/config
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    File file = SPIFFS.open("/config.json", "r");
    if (!file) {
      request->send(500, "application/json", "{\"error\":\"config.json non trovato\"}");
      return;
    }

    String json;
    while (file.available()) json += (char)file.read();
    request->send(200, "application/json", json);
  });

  // HTTP POST /api/config
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
      String body;
      for (size_t i = 0; i < len; i++) body += (char)data[i];
      saveConfigFromJson(body);
      request->send(200, "application/json", "{\"status\":\"salvato\"}");
    });

  Serial.println("[🌐] API configurazione disponibile su /api/config");
}

void handleMqttConfigCommands(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == "bonsai/command/config/update") {
    saveConfigFromJson(msg);  // Riavvia dopo salvataggio
  } else if (String(topic) == "bonsai/command/restart") {
    ESP.restart();
  }
}
