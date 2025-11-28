#pragma once

#define USE_LITTLEFS 0  // 1 = LittleFS, 0 = SPIFFS

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>

#if USE_LITTLEFS
  #include <LittleFS.h>
  #define FS_IMPL LittleFS
#else
  #include <SPIFFS.h>
  #define FS_IMPL SPIFFS
#endif

#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include "config.h"

// ---- extern dal tuo progetto
extern AsyncWebServer server;
extern PubSubClient mqttClient;
extern String deviceId;
extern Config config;

// Path del file config
extern const char* CONFIG_PATH;

// Helpers FS
bool mountFS(bool formatOnFail = false);
bool writeFileAtomic(const char* path, const String& data);
bool readFileToString(const char* path, String& out);

// JSON <-> Config
String configToJson(const Config& c);
bool jsonToConfig(const String& json, Config& out);

// Persistenza
bool saveConfigStruct(const Config& c);
bool loadConfig(Config& out);

// Confronto parametri MQTT critici
bool mqttCriticalChanged(const Config& a, const Config& b);

// Applica + salva + ack + reboot
bool applyAndPersistConfigJson(const String& json, bool rebootAfter = true);

// API HTTP
void setupConfigApi();

// Gestione MQTT config
void handleMqttConfigCommands(char* topic, byte* payload, unsigned int length);
