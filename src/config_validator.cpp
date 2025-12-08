#include "config_validator.h"
#include <Arduino.h>

// Configurazione di default
Config getDefaultConfig() {
  Config def;
  
  // WiFi - vuoto, deve essere configurato
  def.wifi_ssid = "";
  def.wifi_password = "";
  
  // MQTT - vuoto, deve essere configurato
  def.mqtt_broker = "";
  def.mqtt_port = 1883;
  def.mqtt_username = "";
  def.mqtt_password = "";
  
  // Hardware - valori di default sicuri
  def.led_pin = 4;
  def.sensor_pin = 32;
  def.pump_pin = 26;
  def.relay_pin = 27;
  def.battery_pin = 34;
  
  // Logica irrigazione
  def.moisture_threshold = 25;
  def.pump_duration = 5;
  def.measurement_interval = 1800000;  // 30 minuti
  def.use_pump = true;
  def.debug = false;
  def.enable_webserver = false;  // Disabilitato di default per risparmio energia
  def.webserver_timeout = 60;  // 60 secondi
  
  // Sleep
  def.sleep_hours = 0;  // No sleep di default
  
  // Rete statica
  def.use_dhcp = true;
  def.ip_address = "";
  def.gateway = "";
  def.subnet = "";
  
  // OTA / Update
  def.ota_manifest_url = "";
  def.update_server = "";
  def.config_version = "";
  
  // Timezone
  def.timezone = "Europe/Rome";
  
  return def;
}

// Validazione configurazione
bool validateConfig(const Config& config) {
  // Validazione pin GPIO (ESP32 ha pin 0-39, alcuni riservati)
  if (config.led_pin < 0 || config.led_pin > 39) return false;
  if (config.sensor_pin < 0 || config.sensor_pin > 39) return false;
  if (config.pump_pin < 0 || config.pump_pin > 39) return false;
  if (config.relay_pin < 0 || config.relay_pin > 39) return false;
  if (config.battery_pin < 0 || config.battery_pin > 39) return false;
  
  // Pin riservati ESP32 (non utilizzabili)
  int reservedPins[] = {6, 7, 8, 9, 10, 11};  // Flash/PSRAM
  int reservedCount = sizeof(reservedPins) / sizeof(reservedPins[0]);
  
  for (int i = 0; i < reservedCount; i++) {
    if (config.led_pin == reservedPins[i] ||
        config.sensor_pin == reservedPins[i] ||
        config.pump_pin == reservedPins[i] ||
        config.relay_pin == reservedPins[i] ||
        config.battery_pin == reservedPins[i]) {
      return false;
    }
  }
  
  // Validazione valori logici
  if (config.moisture_threshold < 0 || config.moisture_threshold > 100) return false;
  if (config.pump_duration < 0 || config.pump_duration > 3600) return false;  // Max 1 ora
  if (config.measurement_interval < 0) return false;
  if (config.sleep_hours < 0 || config.sleep_hours > 24) return false;
  if (config.webserver_timeout < 0) return false;
  
  // Validazione MQTT port
  if (config.mqtt_port < 1 || config.mqtt_port > 65535) return false;
  
  // Validazione base: almeno WiFi SSID deve essere presente
  if (config.wifi_ssid.length() == 0) return false;
  
  return true;
}

