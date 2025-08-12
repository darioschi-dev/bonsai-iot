
#include "config.h"

bool loadConfig(Config &config)
{
  if (!SPIFFS.begin(true, "/spiffs"))
  {
    Serial.println("[✖] SPIFFS mount failed!");
    return false;
  }

  File file = SPIFFS.open("/config.json", "r");
  if (!file)
  {
    Serial.println("[❌] config.json non trovato su SPIFFS.");
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
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
  if (!config.use_dhcp)
  {
    if (config.ip_address.isEmpty() || config.gateway.isEmpty() || config.subnet.isEmpty())
    {
      Serial.println("[❌] Configurazione statica IP incompleta.");
      return false;
    }
  }
  if (doc.containsKey("ota_manifest_url"))
  {
    config.ota_manifest_url = doc["ota_manifest_url"].as<String>();
  }
  config.update_server = doc["update_server"] | "";
  config.config_version = doc["config_version"] | "";

  file.close();
  // Log the loaded configuration
  Serial.println("[✅] Configurazione caricata con successo:");
  serializeJsonPretty(doc, Serial);
  Serial.println();

  return true;
}

bool saveConfig(const Config &cfg) {
  File file = SPIFFS.open("/config.json", "w");
  if (!file) {
    Serial.println("[❌] Impossibile aprire config.json per scrittura");
    return false;
  }

  StaticJsonDocument<1024> doc;
  doc["wifi_ssid"]            = cfg.wifi_ssid;
  doc["wifi_password"]        = cfg.wifi_password;
  doc["mqtt_broker"]          = cfg.mqtt_broker;
  doc["mqtt_port"]            = cfg.mqtt_port;
  doc["mqtt_username"]        = cfg.mqtt_username;
  doc["mqtt_password"]        = cfg.mqtt_password;
  doc["sensor_pin"]           = cfg.sensor_pin;
  doc["pump_pin"]             = cfg.pump_pin;
  doc["relay_pin"]            = cfg.relay_pin;
  doc["battery_pin"]          = cfg.battery_pin;
  doc["moisture_threshold"]   = cfg.moisture_threshold;
  doc["pump_duration"]        = cfg.pump_duration;
  doc["measurement_interval"] = cfg.measurement_interval;
  doc["use_pump"]              = cfg.use_pump;
  doc["debug"]                 = cfg.debug;
  doc["sleep_hours"]           = cfg.sleep_hours;
  doc["use_dhcp"]              = cfg.use_dhcp;
  doc["ip_address"]            = cfg.ip_address;
  doc["gateway"]               = cfg.gateway;
  doc["subnet"]                = cfg.subnet;

  if (serializeJson(doc, file) == 0) {
    Serial.println("[❌] Errore scrittura JSON su file");
    file.close();
    return false;
  }

  file.close();
  Serial.println("[✅] Config salvata su SPIFFS");
  return true;
}
