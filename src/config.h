#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

struct Config
{

  // 📶 Configurazione Wi-Fi
  String wifi_ssid;     // Nome della rete Wi-Fi
  String wifi_password; // Password della rete Wi-Fi

  // 🌐 Parametri MQTT
  String mqtt_username; // Username per autenticazione MQTT
  String mqtt_password; // Password per autenticazione MQTT
  String mqtt_broker;   // Indirizzo del broker MQTT (es. HiveMQ)
  int mqtt_port;        // Porta del broker MQTT (es. 8883 per MQTTs)

  // 💡 Gestione hardware: LED, sensore, pompa, relè
  int led_pin;     // Pin GPIO per il LED di stato
  int sensor_pin;  // Pin GPIO collegato al sensore di umidità
  int pump_pin;    // Pin GPIO per controllare la pompa (es. tramite transistor)
  int relay_pin;   // Pin GPIO per controllare il relè (alimentazione pompa)
  int battery_pin; // Pin ADC per misurare la tensione batteria (partitore resistivo)

  // 🌱 Parametri logici per irrigazione
  int moisture_threshold;   // Soglia percentuale di umidità sotto la quale attivare la pompa
  int pump_duration;        // Durata in secondi di attivazione della pompa
  int measurement_interval; // Intervallo tra due misurazioni in millisecondi (es. 1800000 = 30 min)
  bool use_pump;            // Abilita/disabilita il controllo automatico della pompa
  bool debug;               // Abilita stampa su seriale per debug

  // 😴 Modalità sleep (deep sleep o delay prolungato)
  int sleep_hours; // Ore di sleep tra una misurazione e l'altra (0 = disabilitato)

  // 🌐 Configurazione di rete statica (se DHCP disattivato)
  String ip_address; // IP statico da assegnare
  String gateway;    // Gateway di rete
  String subnet;     // Subnet mask
  bool use_dhcp;     // Se true, usa DHCP per ottenere IP

  String ota_manifest_url; // URL del manifest OTA
  String update_server;    // Base URL del server config
  String config_version;   // Versione attuale della configurazione
};

bool loadConfig(Config &config);
bool saveConfig(const Config &cfg);
