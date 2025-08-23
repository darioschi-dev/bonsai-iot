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

unsigned long lastMqttPublish = 0;
const unsigned long mqttInterval = 15000; // 15s

extern Config config;
extern int soilValue;
extern int soilPercent;

extern String deviceId;

// Puntatori client MQTT
extern WiFiClient *plainClient;
extern WiFiClientSecure *secureClient;
extern PubSubClient mqttClient;

// ------- Utils -------
// ------- Time utils -------
static inline bool timeIsValid()
{
  time_t nowSec;
  return (time(&nowSec) && nowSec > 1700000000); // ~2023+
}

static inline unsigned long long epochMs()
{
  time_t nowSec;
  if (time(&nowSec) && nowSec > 1700000000)
  {
    return (unsigned long long)nowSec * 1000ULL;
  }
  return 0; // <-- niente fallback a millis() per evitare 1970
}

// ---------------- Device ID ----------------

static inline void setupDeviceId()
{
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId.toLowerCase();
  deviceId = "bonsai-" + deviceId;
}

static inline void publishMqtt(const char *topic, const String &payload, bool retain = false)
{
  if (mqttClient.connected())
  {
    mqttClient.publish(topic, payload.c_str(), retain);
  }
}

// ---------------- CONFIG via MQTT ----------------
static inline bool applyConfigJson(const String &json)
{
  StaticJsonDocument<1024> doc;
  auto err = deserializeJson(doc, json);
  if (err)
  {
    Serial.printf("[CONFIG] Errore parsing JSON: %s\n", err.c_str());
    return false;
  }

  if (doc.containsKey("wifi_ssid"))
    config.wifi_ssid = doc["wifi_ssid"].as<String>();
  if (doc.containsKey("wifi_password"))
    config.wifi_password = doc["wifi_password"].as<String>();
  if (doc.containsKey("mqtt_broker"))
    config.mqtt_broker = doc["mqtt_broker"].as<String>();
  if (doc.containsKey("mqtt_port"))
    config.mqtt_port = doc["mqtt_port"].as<int>();
  if (doc.containsKey("mqtt_username"))
    config.mqtt_username = doc["mqtt_username"].as<String>();
  if (doc.containsKey("mqtt_password"))
    config.mqtt_password = doc["mqtt_password"].as<String>();
  if (doc.containsKey("sensor_pin"))
    config.sensor_pin = doc["sensor_pin"].as<int>();
  if (doc.containsKey("pump_pin"))
    config.pump_pin = doc["pump_pin"].as<int>();
  if (doc.containsKey("relay_pin"))
    config.relay_pin = doc["relay_pin"].as<int>();
  if (doc.containsKey("battery_pin"))
    config.battery_pin = doc["battery_pin"].as<int>();
  if (doc.containsKey("moisture_threshold"))
    config.moisture_threshold = doc["moisture_threshold"].as<int>();
  if (doc.containsKey("pump_duration"))
    config.pump_duration = doc["pump_duration"].as<int>();
  if (doc.containsKey("measurement_interval"))
    config.measurement_interval = doc["measurement_interval"].as<int>();
  if (doc.containsKey("use_pump"))
    config.use_pump = doc["use_pump"].as<bool>();
  if (doc.containsKey("debug"))
    config.debug = doc["debug"].as<bool>();
  if (doc.containsKey("sleep_hours"))
    config.sleep_hours = doc["sleep_hours"].as<int>();
  if (doc.containsKey("use_dhcp"))
    config.use_dhcp = doc["use_dhcp"].as<bool>();
  if (doc.containsKey("ip_address"))
    config.ip_address = doc["ip_address"].as<String>();
  if (doc.containsKey("gateway"))
    config.gateway = doc["gateway"].as<String>();
  if (doc.containsKey("subnet"))
    config.subnet = doc["subnet"].as<String>();
  if (doc.containsKey("ota_manifest_url"))
    config.ota_manifest_url = doc["ota_manifest_url"].as<String>();
  if (doc.containsKey("update_server"))
    config.update_server = doc["update_server"].as<String>();
  if (doc.containsKey("config_version"))
    config.config_version = doc["config_version"].as<String>();

  if (!saveConfig(config))
  {
    Serial.println("[CONFIG] Salvataggio fallito");
    return false;
  }
  return true;
}

// ---------------- CALLBACK MQTT ----------------
static inline void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // 1) Costruisci prima il payload testuale
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i)
    message = (char)payload[i];

  // 2) Debug
  if (config.debug)
    Serial.printf("[MQTT] Topic: %s | Msg: %s\n", topic, message.c_str());

  // 3) Normalizza topic e payload
  const String t(topic);
  String msg = message;
  msg.trim();
  String msgLower = msg;
  msgLower.toLowerCase();

  // 4) Accetta anche payload JSON (es. {"pump":"on"})
  if (msg.startsWith("{"))
  {
    StaticJsonDocument<128> j;
    DeserializationError je = deserializeJson(j, msg);
    if (!je && j.containsKey("pump"))
    {
      msgLower = String(j["pump"].as<const char *>());
      msgLower.toLowerCase();
    }
  }

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
    const bool turnOn = (msgLower == "on");
    const bool turnOff = (msgLower == "off");
    if (!turnOn && !turnOff)
      return;
    if (turnOn)
    {
      digitalWrite(config.pump_pin, LOW);
      publishMqtt("bonsai/status/pump", "on", true);

      // turnOnPump()
      char buf[32];
      unsigned long long ms = epochMs();
      if (ms > 0)
      { // <-- pubblica solo se tempo valido
        snprintf(buf, sizeof(buf), "%llu", ms);
        publishMqtt("bonsai/status/last_on", buf, true);
      }
    }
    else
    {
      digitalWrite(config.pump_pin, HIGH);
      publishMqtt("bonsai/status/pump", "off", true);
    }
    return;
  }

  // Reboot / Restart
  if (t == "bonsai/command/reboot" || t == "bonsai/command/restart")
  {
    ESP.restart();
    return;
  }

  // OTA trigger
  if (t == "bonsai/ota/available" || t == ("bonsai/ota/force/" + deviceId))
  {
    extern void triggerFirmwareCheck();
    triggerFirmwareCheck();
    return;
  }

  // Handler legacy
  handleMqttConfigCommands(topic, payload, length);
}

// ---------------- CONNESSIONE ----------------
static inline void connectMqtt()
{
  mqttClient.setServer(config.mqtt_broker.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected())
  {
    Serial.printf("[MQTT] Connessione a %s:%d...\n", config.mqtt_broker.c_str(), config.mqtt_port);

    // PubSubClient non ha setWill(): il LWT si passa qui
    // connect(clientID, user, pass, willTopic, willQoS, willRetain, willMessage)
    bool ok = mqttClient.connect(
        deviceId.c_str(),
        config.mqtt_username.c_str(),
        config.mqtt_password.c_str(),
        "bonsai/status/online", // will topic
        1,                      // QoS
        true,                   // retain
        "0"                     // will payload (offline)
    );

    if (ok)
    {
      Serial.println("✅ MQTT connesso!");
      // Announce online + device id (retained)
      publishMqtt("bonsai/status/online", "1", true);
      publishMqtt("bonsai/status/device_id", deviceId, true);

      // Sottoscrizioni
      mqttClient.subscribe("bonsai/command/pump");
      mqttClient.subscribe("bonsai/command/reboot");
      mqttClient.subscribe("bonsai/command/restart");
      mqttClient.subscribe("bonsai/command/config/update");
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
static inline void loopMqtt()
{
  if (!mqttClient.connected())
    connectMqtt();
  mqttClient.loop();

  const unsigned long now = millis();
  if (now - lastMqttPublish > mqttInterval)
  {
    lastMqttPublish = now;

    if (timeIsValid())
    {
      publishMqtt("bonsai/status/last_seen", String((long long)epochMs()), true);
    }
    publishMqtt("bonsai/status/wifi", String(WiFi.RSSI()));
#ifdef FIRMWARE_VERSION
    publishMqtt("bonsai/info/firmware", FIRMWARE_VERSION, true);
#endif
    publishMqtt("bonsai/status/humidity", String(soilPercent));
    publishMqtt("bonsai/status/temp", String(0));

    // Nota: batteria grezza — da normalizzare (%)
    publishMqtt("bonsai/status/battery", String(analogRead(config.battery_pin)));

    const bool pumpState = (digitalRead(config.pump_pin) == LOW);
    publishMqtt("bonsai/status/pump", pumpState ? "on" : "off", true);
  }
}

// ---------------- SETUP ----------------
static inline void setupMqtt()
{
  setupDeviceId();

  if (config.mqtt_port == 1883)
  {
    plainClient = new WiFiClient();
    mqttClient.setClient(*plainClient);
  }
  else
  {
    secureClient = new WiFiClientSecure();
    secureClient->setInsecure(); // TODO: setCACert(...)
    mqttClient.setClient(*secureClient);
  }

  mqttClient.setCallback(mqttCallback);
  connectMqtt();
}
