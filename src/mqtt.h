#pragma once
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"
#include "version_auto.h"
#include "config_api.h"

unsigned long lastMqttPublish = 0;
const unsigned long mqttInterval = 15000; // 15 secondi

extern Config config;
extern int soilValue;
extern int soilPercent;

static String deviceId;

// Creiamo un puntatore generico al client MQTT
extern WiFiClient *plainClient;
extern WiFiClientSecure *secureClient;
extern PubSubClient mqttClient;

static void setupDeviceId() {
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toLowerCase();
  deviceId = "bonsai-" + deviceId;
}

void publishMqtt(const char *topic, const String &payload, bool retain = false) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, payload.c_str(), retain);
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++)
    message += (char)payload[i];

  if (config.debug) {
    Serial.printf("[MQTT] Topic ricevuto: %s | Messaggio: %s\n", topic, message.c_str());
  }

  if (String(topic) == "bonsai/command/pump") {
    if (message == "on") {
      digitalWrite(config.pump_pin, LOW);
      publishMqtt("bonsai/status/pump", "on", true);
      publishMqtt("bonsai/status/pump/last_on", String(millis()));
    } else if (message == "off") {
      digitalWrite(config.pump_pin, HIGH);
      publishMqtt("bonsai/status/pump", "off", true);
    }
  }
  else if (String(topic) == "bonsai/command/reboot") {
    if (config.debug) Serial.println("[MQTT] Comando reboot ricevuto");
    ESP.restart();
  }
  else if (String(topic) == "bonsai/ota/available" || String(topic) == ("bonsai/ota/force/" + deviceId)) {
    if (config.debug) Serial.println("[MQTT] OTA trigger ricevuto");
    extern void triggerFirmwareCheck();
    triggerFirmwareCheck();
  }
  else {
    handleMqttConfigCommands(topic, payload, length);
  }
}

void connectMqtt() {
  mqttClient.setServer(config.mqtt_broker.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected()) {
    Serial.printf("[MQTT] Connessione a %s:%d...\n", config.mqtt_broker.c_str(), config.mqtt_port);
    if (mqttClient.connect(deviceId.c_str(), config.mqtt_username.c_str(), config.mqtt_password.c_str())) {
      Serial.println("✅ MQTT connesso!");
      mqttClient.subscribe("bonsai/command/pump");
    } else {
      Serial.print("❌ Fallita. Stato: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

void loopMqtt() {
  if (!mqttClient.connected()) connectMqtt();
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastMqttPublish > mqttInterval) {
    lastMqttPublish = now;

    publishMqtt("bonsai/status/last_seen", String(now));
    publishMqtt("bonsai/status/wifi", String(WiFi.RSSI()));
    publishMqtt("bonsai/info/firmware", FIRMWARE_VERSION, true);

    publishMqtt("bonsai/status/humidity", String(soilPercent));
    publishMqtt("bonsai/status/temp", String(0));
    publishMqtt("bonsai/status/battery", String(analogRead(config.battery_pin)));

    bool pumpState = digitalRead(config.pump_pin) == LOW;
    publishMqtt("bonsai/status/pump", pumpState ? "on" : "off", true);
    publishMqtt("bonsai/status/pump/last_on", String(time(nullptr) * 1000));
  }
}

void setupMqtt() {
  setupDeviceId();

  // Se la porta è 1883 → plain, altrimenti TLS
  if (config.mqtt_port == 1883) {
    plainClient = new WiFiClient();
    mqttClient.setClient(*plainClient);
  } else {
    secureClient = new WiFiClientSecure();
    secureClient->setInsecure(); // oppure setCACert(...) se hai la CA
    mqttClient.setClient(*secureClient);
  }

  mqttClient.setCallback(mqttCallback);
  connectMqtt();

  mqttClient.subscribe("bonsai/command/pump");
  mqttClient.subscribe("bonsai/command/reboot");
  mqttClient.subscribe("bonsai/command/config/update");
  mqttClient.subscribe("bonsai/command/restart");
  mqttClient.subscribe("bonsai/ota/available");

  String forceTopic = "bonsai/ota/force/" + deviceId;
  mqttClient.subscribe(forceTopic.c_str());
}
