#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

struct Config {

  // 📶 Configurazione Wi-Fi
  String wifi_ssid;         // Nome della rete Wi-Fi
  String wifi_password;     // Password della rete Wi-Fi

  // 🌐 Parametri MQTT
  String mqtt_username;     // Username per autenticazione MQTT
  String mqtt_password;     // Password per autenticazione MQTT
  String mqtt_broker;       // Indirizzo del broker MQTT (es. HiveMQ)
  int mqtt_port;            // Porta del broker MQTT (es. 8883 per MQTTs)

  // 💡 Gestione hardware: LED, sensore, pompa, relè
  int led_pin;              // Pin GPIO per il LED di stato
  int sensor_pin;           // Pin GPIO collegato al sensore di umidità
  int pump_pin;             // Pin GPIO per controllare la pompa (es. tramite transistor)
  int relay_pin;            // Pin GPIO per controllare il relè (alimentazione pompa)
  int battery_pin;          // Pin ADC per misurare la tensione batteria (partitore resistivo)

  // 🌱 Parametri logici per irrigazione
  int moisture_threshold;   // Soglia percentuale di umidità sotto la quale attivare la pompa
  int pump_duration;        // Durata in secondi di attivazione della pompa
  int measurement_interval; // Intervallo tra due misurazioni in millisecondi (es. 1800000 = 30 min)
  bool use_pump;            // Abilita/disabilita il controllo automatico della pompa
  bool debug;               // Abilita stampa su seriale per debug

  // 😴 Modalità sleep (deep sleep o delay prolungato)
  int sleep_hours;          // Ore di sleep tra una misurazione e l'altra (0 = disabilitato)

  // 🌐 Configurazione di rete statica (se DHCP disattivato)
  String ip_address;        // IP statico da assegnare
  String gateway;           // Gateway di rete
  String subnet;            // Subnet mask
  bool use_dhcp;            // Se true, usa DHCP per ottenere IP

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
  config.battery_pin = doc["battery_pin"] | 34;
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
