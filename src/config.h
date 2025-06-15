#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

struct Config {
  String wifi_ssid;
  String wifi_password;
  String mqtt_username;
  String mqtt_password;
  String mqtt_broker;
  int mqtt_port;
  int led_pin;
  int sensor_pin;
  int pump_pin;
  int moisture_threshold;
  bool debug;
  bool use_pump;
  int sleep_hours;
  int relay_pin;
  int pump_duration;
  int measurement_interval;
  String ip_address;
  String gateway;
  String subnet;
  bool use_dhcp;
};

bool loadConfig(Config &config) {
  if (!SPIFFS.begin(true, "/spiffs")) {
    Serial.println("[✖] SPIFFS mount failed!");
    return false;
  }

  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println("[❌] config.json non trovato su SPIFFS.");
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print("[❌] Errore parsing JSON: ");
    Serial.println(error.f_str());
    return false;
  }

  config.wifi_ssid = doc["wifi_ssid"] | "";
  config.wifi_password = doc["wifi_password"] | "";
  config.mqtt_username = doc["mqtt_username"] | "";
  config.mqtt_password = doc["mqtt_password"] | "";
  config.mqtt_broker = doc["mqtt_broker"] | "broker.hivemq.com";
  config.mqtt_port = doc["mqtt_port"] | 1883;
  config.led_pin = doc["led_pin"] | 4;
  config.sensor_pin = doc["sensor_pin"] | 32;
  config.pump_pin = doc["pump_pin"] | 26;
  config.moisture_threshold = doc["moisture_threshold"] | 500;
  config.debug = doc["debug"] | false;
  config.use_pump = doc["use_pump"] | true;
  config.sleep_hours = doc["sleep_hours"] | 0;
  config.relay_pin = doc["relay_pin"] | 27;
  config.pump_duration = doc["pump_duration"] | 5;
  config.measurement_interval = doc["measurement_interval"] | 1800000;
  config.use_dhcp = doc["use_dhcp"] | true;
  config.ip_address = doc["ip_address"] | "";
  config.gateway = doc["gateway"] | "";
  config.subnet = doc["subnet"] | "";
  if (!config.use_dhcp) {
    if (config.ip_address.isEmpty() || config.gateway.isEmpty() || config.subnet.isEmpty()) {
      Serial.println("[❌] Configurazione statica IP incompleta.");
      return false;
    }
  }
  file.close();
  // Log the loaded configuration
  Serial.println("[✅] Configurazione caricata con successo:");
  serializeJsonPretty(doc, Serial);
  Serial.println();

  return true;
}
