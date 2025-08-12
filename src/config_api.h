#pragma once

// =============== FS backend ===============
#define USE_LITTLEFS 0  // metti 1 per LittleFS, 0 per SPIFFS

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
extern Config config;
extern PubSubClient mqttClient;
extern String deviceId;   // definito in mqtt.h

// ---- costanti
static const char* CONFIG_PATH = "/config.json";

// =============== Helpers FS ===============
static bool mountFS(bool formatOnFail = false) {
  if (FS_IMPL.begin(formatOnFail)) return true;
  if (!formatOnFail) return false;
  // tenta con format
  return FS_IMPL.begin(true);
}

static bool writeFileAtomic(const char* path, const String& data) {
  String tmp = String(path) + ".tmp";
  File f = FS_IMPL.open(tmp, "w");
  if (!f) return false;
  size_t w = f.print(data);
  f.flush(); f.close();
  if (w != data.length()) { FS_IMPL.remove(tmp); return false; }
  FS_IMPL.remove(path);
  return FS_IMPL.rename(tmp, path);
}

static bool readFileToString(const char* path, String& out) {
  File f = FS_IMPL.open(path, "r");
  if (!f) return false;
  out = f.readString();
  f.close();
  return true;
}

// =============== JSON <-> Config ===============
static String configToJson(const Config& c) {
  StaticJsonDocument<1024> d;
  d["wifi_ssid"] = c.wifi_ssid;
  d["wifi_password"] = c.wifi_password;

  d["mqtt_broker"] = c.mqtt_broker;
  d["mqtt_port"] = c.mqtt_port;
  d["mqtt_username"] = c.mqtt_username;
  d["mqtt_password"] = c.mqtt_password;

  d["sensor_pin"] = c.sensor_pin;
  d["pump_pin"] = c.pump_pin;
  d["relay_pin"] = c.relay_pin;
  d["battery_pin"] = c.battery_pin;

  d["moisture_threshold"] = c.moisture_threshold;
  d["pump_duration"] = c.pump_duration;
  d["measurement_interval"] = c.measurement_interval;

  d["debug"] = c.debug;
  d["use_pump"] = c.use_pump;
  d["sleep_hours"] = c.sleep_hours;

  d["use_dhcp"] = c.use_dhcp;
  d["ip_address"] = c.ip_address;
  d["gateway"] = c.gateway;
  d["subnet"] = c.subnet;

  String out; serializeJson(d, out);
  return out;
}

static bool jsonToConfig(const String& json, Config& out) {
  StaticJsonDocument<1024> d;
  DeserializationError err = deserializeJson(d, json);
  if (err) return false;

  auto pickStr  = [&](const char* k, String& t){ if (d.containsKey(k)) t = d[k].as<String>(); };
  auto pickInt  = [&](const char* k, int& t){ if (d.containsKey(k)) t = d[k].as<int>(); };
  auto pickLong = [&](const char* k, long& t){ if (d.containsKey(k)) t = d[k].as<long>(); };
  auto pickBool = [&](const char* k, bool& t){ if (d.containsKey(k)) t = d[k].as<bool>(); };

  pickStr("wifi_ssid", out.wifi_ssid);
  pickStr("wifi_password", out.wifi_password);

  pickStr("mqtt_broker", out.mqtt_broker);
  pickInt("mqtt_port",   out.mqtt_port);
  pickStr("mqtt_username", out.mqtt_username);
  pickStr("mqtt_password", out.mqtt_password);

  pickInt("sensor_pin", out.sensor_pin);
  pickInt("pump_pin",   out.pump_pin);
  pickInt("relay_pin",  out.relay_pin);
  pickInt("battery_pin", out.battery_pin);

  pickInt("moisture_threshold", out.moisture_threshold);
  pickInt("pump_duration", out.pump_duration);
  pickLong("measurement_interval", out.measurement_interval);

  pickBool("debug", out.debug);
  pickBool("use_pump", out.use_pump);
  pickInt("sleep_hours", out.sleep_hours);

  pickBool("use_dhcp", out.use_dhcp);
  pickStr("ip_address", out.ip_address);
  pickStr("gateway", out.gateway);
  pickStr("subnet", out.subnet);

  return true;
}

// =============== Persistenza + Apply ===============
static bool saveConfigStruct(const Config& c) {
  String json = configToJson(c);
  return writeFileAtomic(CONFIG_PATH, json);
}

// ritorna true se cambiati parametri critici MQTT
static bool mqttCriticalChanged(const Config& a, const Config& b) {
  return a.mqtt_broker != b.mqtt_broker ||
         a.mqtt_port   != b.mqtt_port   ||
         a.mqtt_username != b.mqtt_username ||
         a.mqtt_password != b.mqtt_password;
}

// Applica JSON -> config, salva su FS, e gestisce ack e reboot
static bool applyAndPersistConfigJson(const String& json, bool rebootAfter = true) {
  Config newCfg = config; // parte dagli attuali
  if (!jsonToConfig(json, newCfg)) {
    Serial.println(F("[CONFIG] JSON non valido"));
    mqttClient.publish("bonsai/ack/config", "{\"status\":\"failed\",\"reason\":\"parse\"}");
    return false;
  }

  bool critChange = mqttCriticalChanged(config, newCfg);

  // persist
  if (!saveConfigStruct(newCfg)) {
    Serial.println(F("[CONFIG] Salvataggio atomico fallito"));
    mqttClient.publish("bonsai/ack/config", "{\"status\":\"failed\",\"reason\":\"fs_write\"}");
    return false;
  }

  // swap in RAM
  config = newCfg;
  mqttClient.publish("bonsai/ack/config", "{\"status\":\"applied\"}");

  if (rebootAfter) {
    Serial.println(F("[CONFIG] Salvato. Riavvio..."));
    delay(500);
    ESP.restart();
  } else if (critChange) {
    // opzionale: riconnetti MQTT qui se non riavvii
    Serial.println(F("[CONFIG] Parametri MQTT cambiati: forzare reconnect/reboot consigliato."));
  }
  return true;
}

// =============== API HTTP ===============
static void setupConfigApi() {
  if (!mountFS(false)) {
    Serial.println(F("[FS] Mount fallito, provo format..."));
    mountFS(true);
  }

  // GET /api/config
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req){
    String json;
    if (readFileToString(CONFIG_PATH, json)) {
      req->send(200, "application/json", json);
    } else {
      // se non esiste, restituisci la config attuale in RAM
      req->send(200, "application/json", configToJson(config));
    }
  });

  // POST /api/config  (save+reboot di default)
  server.on(
    "/api/config",
    HTTP_POST,
    [](AsyncWebServerRequest* req){}, nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
      String body; body.reserve(len);
      for (size_t i=0; i<len; i++) body += (char)data[i];

      bool reboot = true;
      if (req->hasParam("reboot", true)) {
        reboot = req->getParam("reboot", true)->value() != "0";
      }

      bool ok = applyAndPersistConfigJson(body, reboot);
      if (ok) req->send(200, "application/json", "{\"status\":\"applied\"}");
      else    req->send(400, "application/json", "{\"error\":\"invalid_or_write_failed\"}");
    }
  );

  Serial.println(F("[🌐] API configurazione su /api/config (GET/POST)"));
}

// =============== MQTT handler legacy/nuovo ===============
static void handleMqttConfigCommands(char* topic, byte* payload, unsigned int length) {
  String msg; msg.reserve(length);
  for (unsigned int i=0; i<length; i++) msg += (char)payload[i];
  String t(topic);

  // Nuovo: config retained globale o per-device
  if (t == "bonsai/config" || t == ("bonsai/config/" + deviceId)) {
    applyAndPersistConfigJson(msg, /*rebootAfter*/ true);
    return;
  }

  // Legacy: comandi espliciti
  if (t == "bonsai/command/config/update") {
    applyAndPersistConfigJson(msg, /*rebootAfter*/ true);
  } else if (t == "bonsai/command/restart") {
    ESP.restart();
  }
}
