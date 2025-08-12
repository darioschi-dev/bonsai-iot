#pragma once
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "version_auto.h"
#include "config_api.h"
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

unsigned long lastMqttPublish = 0;
const unsigned long mqttInterval = 15000; // 15 secondi

extern Config config;
extern int soilValue;
extern int soilPercent;

extern String deviceId;

// Puntatori client MQTT
extern WiFiClient *plainClient;
extern WiFiClientSecure *secureClient;
extern PubSubClient mqttClient;

static void setupDeviceId()
{
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toLowerCase();
  deviceId = "bonsai-" + deviceId;
}

void publishMqtt(const char *topic, const String &payload, bool retain = false)
{
  if (mqttClient.connected())
  {
    mqttClient.publish(topic, payload.c_str(), retain);
  }
}

// ---------------- CONFIG via MQTT ----------------

bool applyConfigJson(const String &json) {
    StaticJsonDocument<1024> doc;
    auto err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[CONFIG] Errore parsing JSON: %s\n", err.c_str());
        return false;
    }

    if (doc.containsKey("wifi_ssid"))      config.wifi_ssid      = doc["wifi_ssid"].as<String>();
    if (doc.containsKey("wifi_password"))  config.wifi_password  = doc["wifi_password"].as<String>();
    if (doc.containsKey("mqtt_broker"))    config.mqtt_broker    = doc["mqtt_broker"].as<String>();
    if (doc.containsKey("mqtt_port"))      config.mqtt_port      = doc["mqtt_port"].as<int>();
    if (doc.containsKey("mqtt_username"))  config.mqtt_username  = doc["mqtt_username"].as<String>();
    if (doc.containsKey("mqtt_password"))  config.mqtt_password  = doc["mqtt_password"].as<String>();
    if (doc.containsKey("sensor_pin"))     config.sensor_pin     = doc["sensor_pin"].as<int>();
    if (doc.containsKey("pump_pin"))       config.pump_pin       = doc["pump_pin"].as<int>();
    if (doc.containsKey("relay_pin"))      config.relay_pin      = doc["relay_pin"].as<int>();
    if (doc.containsKey("battery_pin"))    config.battery_pin    = doc["battery_pin"].as<int>();
    if (doc.containsKey("moisture_threshold")) config.moisture_threshold = doc["moisture_threshold"].as<int>();
    if (doc.containsKey("pump_duration"))  config.pump_duration  = doc["pump_duration"].as<int>();
    if (doc.containsKey("measurement_interval")) config.measurement_interval = doc["measurement_interval"].as<int>();
    if (doc.containsKey("use_pump"))       config.use_pump       = doc["use_pump"].as<bool>();
    if (doc.containsKey("debug"))          config.debug          = doc["debug"].as<bool>();
    if (doc.containsKey("sleep_hours"))    config.sleep_hours    = doc["sleep_hours"].as<int>();
    if (doc.containsKey("use_dhcp"))       config.use_dhcp       = doc["use_dhcp"].as<bool>();
    if (doc.containsKey("ip_address"))     config.ip_address     = doc["ip_address"].as<String>();
    if (doc.containsKey("gateway"))        config.gateway        = doc["gateway"].as<String>();
    if (doc.containsKey("subnet"))         config.subnet         = doc["subnet"].as<String>();
    if (doc.containsKey("ota_manifest_url")) config.ota_manifest_url = doc["ota_manifest_url"].as<String>();
    if (doc.containsKey("update_server"))    config.update_server    = doc["update_server"].as<String>();
    if (doc.containsKey("config_version"))   config.config_version   = doc["config_version"].as<String>();

    // Salvataggio persistente
    if (!saveConfig(config)) {
        Serial.println("[CONFIG] Salvataggio fallito");
        return false;
    }
    return true;
}

// ---------------- CALLBACK MQTT ----------------
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++)
    message += (char)payload[i];

  if (config.debug)
  {
    Serial.printf("[MQTT] Topic: %s | Messaggio: %s\n", topic, message.c_str());
  }

  String t(topic);

  // Config globale o per-device
  if (t == "bonsai/config" || t == "bonsai/config/" + deviceId)
  {
    bool ok = applyConfigJson(message);
    publishMqtt("bonsai/ack/config", ok ? "{\"status\":\"applied\"}" : "{\"status\":\"failed\"}");
    if (ok)
    {
      delay(1000);
      ESP.restart();
    }
    return;
  }

  // Comando pompa
  if (t == "bonsai/command/pump")
  {
    if (message == "on")
    {
      digitalWrite(config.pump_pin, LOW);
      publishMqtt("bonsai/status/pump", "on", true);
      publishMqtt("bonsai/status/last_on", String((long long)time(nullptr) * 1000), true); // epoch ms
    }
    else if (message == "off")
    {
      digitalWrite(config.pump_pin, HIGH);
      publishMqtt("bonsai/status/pump", "off", true);
    }
  }
  // Reboot
  else if (t == "bonsai/command/reboot")
  {
    ESP.restart();
  }
  // OTA trigger
  else if (t == "bonsai/ota/available" || t == ("bonsai/ota/force/" + deviceId))
  {
    extern void triggerFirmwareCheck();
    triggerFirmwareCheck();
  }
  // Handler legacy
  else
  {
    handleMqttConfigCommands(topic, payload, length);
  }
}

// ---------------- CONNESSIONE ----------------
void connectMqtt()
{
  mqttClient.setServer(config.mqtt_broker.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected())
  {
    Serial.printf("[MQTT] Connessione a %s:%d...\n", config.mqtt_broker.c_str(), config.mqtt_port);
    if (mqttClient.connect(deviceId.c_str(), config.mqtt_username.c_str(), config.mqtt_password.c_str()))
    {
      Serial.println("✅ MQTT connesso!");
      // Sottoscrizioni
      mqttClient.subscribe("bonsai/command/pump");
      mqttClient.subscribe("bonsai/command/reboot");
      mqttClient.subscribe("bonsai/command/config/update");
      mqttClient.subscribe("bonsai/command/restart");
      mqttClient.subscribe("bonsai/ota/available");
      mqttClient.subscribe(("bonsai/ota/force/" + deviceId).c_str());
      mqttClient.subscribe("bonsai/config");
      mqttClient.subscribe(("bonsai/config/" + deviceId).c_str());
    }
    else
    {
      Serial.print("❌ Fallita. Stato: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

// ---------------- LOOP ----------------
void loopMqtt()
{
  if (!mqttClient.connected())
    connectMqtt();
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastMqttPublish > mqttInterval)
  {
    lastMqttPublish = now;

    publishMqtt("bonsai/status/last_seen", String((long long)time(nullptr) * 1000), true);
    publishMqtt("bonsai/status/wifi", String(WiFi.RSSI()));
    publishMqtt("bonsai/info/firmware", FIRMWARE_VERSION, true);

    publishMqtt("bonsai/status/humidity", String(soilPercent));
    publishMqtt("bonsai/status/temp", String(0));
    publishMqtt("bonsai/status/battery", String(analogRead(config.battery_pin)));

    bool pumpState = digitalRead(config.pump_pin) == LOW;
    publishMqtt("bonsai/status/pump", pumpState ? "on" : "off", true);
  }
}

// ---------------- SETUP ----------------
void setupMqtt()
{
  setupDeviceId();

  // Plain o TLS
  if (config.mqtt_port == 1883)
  {
    plainClient = new WiFiClient();
    mqttClient.setClient(*plainClient);
  }
  else
  {
    secureClient = new WiFiClientSecure();
    secureClient->setInsecure(); // oppure setCACert(...)
    mqttClient.setClient(*secureClient);
  }

  mqttClient.setCallback(mqttCallback);
  connectMqtt();
}
