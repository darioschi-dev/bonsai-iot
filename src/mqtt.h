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

// ---- util: decide se è una config "nuova" ----
static inline bool isNewerConfigVersion(const String &incoming, const String &current)
{
  if (incoming.length() == 0)
    return false; // niente versione => non auto-applicare
  if (current.length() == 0)
    return true;
  // timestamp YYYYMMDDHHmmss o ULID/stringa: confronto lessicografico va bene se monotono
  return incoming != current && incoming > current;
}

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

// ---------------- PUBLISH CONFIG SNAPSHOT ----------------
static inline void publishConfigSnapshot()
{
  // serializza l’intera config corrente e pubblica RETAINED
  StaticJsonDocument<1024> doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["mqtt_broker"] = config.mqtt_broker;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_username"] = config.mqtt_username;
  doc["mqtt_password"] = config.mqtt_password;
  doc["sensor_pin"] = config.sensor_pin;
  doc["pump_pin"] = config.pump_pin;
  doc["relay_pin"] = config.relay_pin;
  doc["battery_pin"] = config.battery_pin;
  doc["moisture_threshold"] = config.moisture_threshold;
  doc["pump_duration"] = config.pump_duration;
  doc["measurement_interval"] = config.measurement_interval;
  doc["use_pump"] = config.use_pump;
  doc["debug"] = config.debug;
  doc["sleep_hours"] = config.sleep_hours;
  doc["use_dhcp"] = config.use_dhcp;
  doc["ip_address"] = config.ip_address;
  doc["gateway"] = config.gateway;
  doc["subnet"] = config.subnet;
  doc["ota_manifest_url"] = config.ota_manifest_url;
  doc["update_server"] = config.update_server;
<<<<<<< HEAD
  doc["config_version"] = config.config_version;
=======
  doc["config_version"] = config.config_version; // opzionale
  doc["config_version"] = config.config_version; // <- chiave
>>>>>>> 0397a71b4dd7f10c9015b6e8c98526bea5650221
  doc["device_id"] = deviceId;

  String out;
  out.reserve(1024);
  serializeJson(doc, out);
  publishMqtt("bonsai/config", out, /*retain=*/true);
}

// ---------------- CALLBACK MQTT ----------------
static inline void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // 1) Ricostruisci il payload in una String
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // 2) Debug
  if (config.debug) {
    Serial.printf("[MQTT] Topic: %s | Msg: %s\n", topic, message.c_str());
  }

  // 3) Normalizza topic e payload (una sola dichiarazione!)
  const String t(topic);
  String msg = message; 
  msg.trim();
  String msgLower = msg; 
  msgLower.toLowerCase();

  // 4) Accetta anche payload JSON (es. {"pump":"on"})
  if (msg.startsWith("{")) {
    StaticJsonDocument<128> j;
    DeserializationError je = deserializeJson(j, msg);
    if (!je && j.containsKey("pump")) {
      msgLower = String(j["pump"].as<const char*>());
      msgLower.toLowerCase();
    }
  }

  // ===== CONFIG: comando live (non retained)
  if (t == "bonsai/config/set" || t == ("bonsai/config/set/" + deviceId)) {
    const bool ok = applyConfigJson(msg);
    publishMqtt("bonsai/config/ack", ok ? "{\"ok\":true,\"source\":\"set\"}" : "{\"ok\":false,\"source\":\"set\"}");
    if (ok) {
      publishConfigSnapshot();     // aggiorna lo snapshot retained
      delay(300);
      ESP.restart();               // riavvio consigliato
    }
    return;
  }

  // ===== CONFIG: snapshot retained (mailbox)
  if (t == "bonsai/config" || t == ("bonsai/config/" + deviceId)) {
    StaticJsonDocument<256> j;
    if (deserializeJson(j, msg) == DeserializationError::Ok) {
      const String incomingVer = j["config_version"] | "";
      if (isNewerConfigVersion(incomingVer, config.config_version)) {
        const bool ok = applyConfigJson(msg);
        publishMqtt("bonsai/config/ack", ok ? "{\"ok\":true,\"source\":\"retained\"}" : "{\"ok\":false,\"source\":\"retained\"}");
        if (ok) {
          publishConfigSnapshot();
          delay(300);
          ESP.restart();
        }
      } else {
        if (config.debug) Serial.println("[CONFIG] Retained uguale/vecchio: ignorato");
      }
    } else {
      if (config.debug) Serial.println("[CONFIG] Retained non-JSON: ignorato");
    }
    return;
  }

  // ===== Comando pompa
  if (t == "bonsai/command/pump") {
    const bool turnOn  = (msgLower == "on");
    const bool turnOff = (msgLower == "off");
    if (!turnOn && !turnOff) return;

    if (turnOn) {
      digitalWrite(config.pump_pin, LOW);
      publishMqtt("bonsai/status/pump", "on", true);

      // pubblica last_on solo se l'ora è valida
      const unsigned long long ms = epochMs();
      if (ms > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", ms);
        publishMqtt("bonsai/status/last_on", String(buf), true);
      }
    } else {
      digitalWrite(config.pump_pin, HIGH);
      publishMqtt("bonsai/status/pump", "off", true);
    }
    return;
  }

  // ===== Reboot / restart
  if (t == "bonsai/command/reboot" || t == "bonsai/command/restart") {
    ESP.restart();
    return;
  }

  // ===== OTA trigger
  if (t == "bonsai/ota/available" || t == ("bonsai/ota/force/" + deviceId)) {
    extern void triggerFirmwareCheck();
    triggerFirmwareCheck();
    return;
  }

  // ===== Handler legacy (se esiste ancora)
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
      publishMqtt("bonsai/status/online", "1", true);
      publishMqtt("bonsai/status/device_id", deviceId, true);

      // Sottoscrizioni COMANDI (mai retained)
      mqttClient.subscribe("bonsai/command/pump");
      mqttClient.subscribe("bonsai/command/reboot");
      mqttClient.subscribe("bonsai/command/restart");
      mqttClient.subscribe("bonsai/command/config/update"); // se lo usi ancora

      // OTA
      mqttClient.subscribe("bonsai/ota/available");
      mqttClient.subscribe(("bonsai/ota/force/" + deviceId).c_str());

      // CONFIG: set (comando) + per-device
      mqttClient.subscribe("bonsai/config/set");
      mqttClient.subscribe(("bonsai/config/set/" + deviceId).c_str());

      // CONFIG: snapshot (lettura)
      mqttClient.subscribe("bonsai/config");
      mqttClient.subscribe(("bonsai/config/" + deviceId).c_str());

      // (facoltativo) pubblica lo snapshot appena connesso
      publishConfigSnapshot();
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
