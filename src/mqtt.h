#pragma once
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"
#include "version_auto.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastMqttPublish = 0;
const unsigned long mqttInterval = 15000; // 15 secondi

extern Config config;
extern int soilValue;
extern int soilPercent;

void publishMqtt(const char* topic, const String& payload, bool retain = false) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, payload.c_str(), retain);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

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
}

void connectMqtt() {
  mqttClient.setServer(config.mqtt_broker.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Connessione in corso...");
    if (mqttClient.connect("bonsai-client", config.mqtt_username.c_str(), config.mqtt_password.c_str())) {
      Serial.println(" connesso!");
      mqttClient.subscribe("bonsai/command/pump");
    } else {
      Serial.print(" fallita. Stato: ");
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

    publishMqtt("bonsai/status/last_seen", String(millis()));
    publishMqtt("bonsai/status/humidity", String(soilPercent));
    publishMqtt("bonsai/status/pump", digitalRead(config.pump_pin) == LOW ? "on" : "off", true);
    publishMqtt("bonsai/status/wifi", String(WiFi.RSSI()));
    publishMqtt("bonsai/status/pump/last_on", String(millis()));
    publishMqtt("bonsai/status/battery", String(analogRead(A13)));
    publishMqtt("bonsai/status/temp", String(0)); // placeholder
    publishMqtt("bonsai/info/firmware", FIRMWARE_VERSION, true);
  }
}

void setupMqtt() {
  connectMqtt();
}
