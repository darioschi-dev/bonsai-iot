#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

struct Config
{
  // WiFi
  String wifi_ssid;
  String wifi_password;

  // MQTT
  String mqtt_username;
  String mqtt_password;
  String mqtt_broker;
  int    mqtt_port;

  // Hardware
  int led_pin;
  int sensor_pin;
  int pump_pin;
  int relay_pin;
  int battery_pin;

  // Logica irrigazione
  int moisture_threshold;
  int pump_duration;
  int measurement_interval;
  bool use_pump;
  bool debug;

  // Sleep
  int sleep_hours;

  // Rete statica
  bool   use_dhcp;
  String ip_address;
  String gateway;
  String subnet;

  // OTA / Update
  String ota_manifest_url;   // URL manifest OTA (obbligatorio per OTA firmware+config)
  String update_server;      // Base URL del server (per eventuale /firmware e /config)
  String config_version;     // Versione locale del config salvato
};

bool loadConfig(Config &config);
