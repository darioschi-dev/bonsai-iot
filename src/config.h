#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#define PUMP_ON LOW
#define PUMP_OFF HIGH

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
  bool enable_webserver;  // Abilita webserver locale (consuma risorse)
  int webserver_timeout; // Seconds to wait before deep sleep

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
  
  // Timezone
  String timezone;           // Timezone string (IANA: "Europe/Rome", "Europe/Berlin", "UTC" o POSIX: "CET-1CEST,M3.5.0/2,M10.5.0/3")
};

bool loadConfig(Config &config);
