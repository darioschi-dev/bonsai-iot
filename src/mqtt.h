#pragma once
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>

#include "config.h"
#include "config_api.h"

// =====================
// Variabili esterne
// =====================
extern Config config;
extern int soilValue;
extern int soilPercent;
extern String deviceId;

extern WiFiClient* plainClient;
extern WiFiClientSecure* secureClient;
extern PubSubClient mqttClient;

extern unsigned long lastMqttPublish;
extern const unsigned long mqttInterval;

// =====================
// Funzioni utilità inline
// =====================
static inline bool timeIsValid() {
  time_t nowSec;
  return (time(&nowSec) && nowSec > 1700000000);
}

static inline unsigned long long epochMs() {
  time_t nowSec;
  if (time(&nowSec) && nowSec > 1700000000) {
    return (unsigned long long)nowSec * 1000ULL;
  }
  return 0;
}

static inline void publishMqtt(const String& topic, const String& payload, bool retain = false) {
  if (mqttClient.connected())
    mqttClient.publish(topic.c_str(), payload.c_str(), retain);
}

static inline void setupDeviceId() {
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toLowerCase();
  deviceId = "bonsai-" + deviceId;
}

// =====================
// Prototipi FUNZIONI
// =====================

bool isNewerConfigVersion(const String& incoming, const String& current);
bool applyConfigJson(const String& json);
void publishConfigSnapshot();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void connectMqtt();
void loopMqtt();
void setupMqtt();
