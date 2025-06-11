#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

struct Config {
  String wifi_ssid;
  String wifi_password;
  String mqtt_broker;
  int mqtt_port;
  int led_pin;
  int sensor_pin;
  int pump_pin;
  int moisture_threshold;
  bool debug;
  bool use_pump;
  int sleep_hours;
};

bool loadConfig(Config &config) {
  if (!SPIFFS.begin(true)) return false;
  File file = SPIFFS.open("/config.json", "r");
  if (!file) return false;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) return false;

  config.wifi_ssid = doc["wifi_ssid"].as<String>();
  config.wifi_password = doc["wifi_password"].as<String>();
  config.mqtt_broker = doc["mqtt_broker"].as<String>();
  config.mqtt_port = doc["mqtt_port"];
  config.led_pin = doc["led_pin"];
  config.sensor_pin = doc["sensor_pin"];
  config.pump_pin = doc["pump_pin"];
  config.moisture_threshold = doc["moisture_threshold"];
  config.debug = doc["debug"];
  config.use_pump = doc["use_pump"];
  config.sleep_hours = doc["sleep_hours"];
  return true;
}
