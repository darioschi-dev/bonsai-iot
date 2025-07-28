#pragma once
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"
#include "version_auto.h"
#include "config_api.h"

WiFiClientSecure wifiClient; // TLS client
PubSubClient mqttClient(wifiClient);

unsigned long lastMqttPublish = 0;
const unsigned long mqttInterval = 15000; // 15 secondi

extern Config config;
extern int soilValue;
extern int soilPercent;

void publishMqtt(const char *topic, const String &payload, bool retain = false)
{
  if (mqttClient.connected())
  {
    mqttClient.publish(topic, payload.c_str(), retain);
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String message;
  for (unsigned int i = 0; i < length; i++)
    message += (char)payload[i];

  if (config.debug)
  {
    Serial.printf("[MQTT] Topic ricevuto: %s | Messaggio: %s\n", topic, message.c_str());
  }

  if (String(topic) == "bonsai/command/pump")
  {
    if (message == "on")
    {
      digitalWrite(config.pump_pin, LOW);
      publishMqtt("bonsai/status/pump", "on", true);
      publishMqtt("bonsai/status/pump/last_on", String(millis()));
    }
    else if (message == "off")
    {
      digitalWrite(config.pump_pin, HIGH);
      publishMqtt("bonsai/status/pump", "off", true);
    }
  }
  else if (String(topic) == "bonsai/command/reboot")
  {
    if (config.debug)
    {
      Serial.println("[MQTT] Comando reboot ricevuto");
    }
    ESP.restart();
  }
  else
  {
    handleMqttConfigCommands(topic, payload, length); // da config_api.h
  }
}

void connectMqtt()
{
  mqttClient.setServer(config.mqtt_broker.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  wifiClient.setInsecure(); // <-- accetta qualsiasi certificato (HiveMQ Cloud)

  while (!mqttClient.connected())
  {
    Serial.printf("[MQTT] Connessione a %s:%d...\n", config.mqtt_broker.c_str(), config.mqtt_port);
    if (mqttClient.connect("bonsai-client", config.mqtt_username.c_str(), config.mqtt_password.c_str()))
    {
      Serial.println("✅ MQTT connesso!");
      mqttClient.subscribe("bonsai/command/pump");
    }
    else
    {
      Serial.print("❌ Fallita. Stato: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

void loopMqtt()
{
  if (!mqttClient.connected())
    connectMqtt();

  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastMqttPublish > mqttInterval)
  {
    lastMqttPublish = now;

    // 🟢 Informazioni di stato generiche
    publishMqtt("bonsai/status/last_seen", String(now));
    publishMqtt("bonsai/status/wifi", String(WiFi.RSSI()));
    publishMqtt("bonsai/info/firmware", FIRMWARE_VERSION, true);

    // 🌱 Dati ambientali
    publishMqtt("bonsai/status/humidity", String(soilPercent));
    publishMqtt("bonsai/status/temp", String(0)); // placeholder per temperatura
    publishMqtt("bonsai/status/battery", String(analogRead(config.battery_pin)));

    // 💧 Stato pompa
    bool pumpState = digitalRead(config.pump_pin) == LOW;
    publishMqtt("bonsai/status/pump", pumpState ? "on" : "off", true);

    // Nota: `last_on` viene aggiornato **solo lato frontend** quando si invia un comando manuale via API.
    // Se vuoi anche registrare `last_on` da ESP32 in automatico, va gestito lato codice pompa.
    publishMqtt("bonsai/status/pump/last_on", String(time(nullptr) * 1000));
  }
}

void setupMqtt()
{
  mqttClient.subscribe("bonsai/command/config/update");
  mqttClient.subscribe("bonsai/command/restart");
  connectMqtt();
}
