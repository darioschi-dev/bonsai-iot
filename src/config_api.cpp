#include "config_api.h"
#include <ArduinoJson.h>

const char* CONFIG_PATH = "/config.json";

// --- Helpers FS ---
static bool writeFileAtomic(const char* path, const String& data) {
  String tmp = String(path) + ".tmp";
  File f = FS_IMPL.open(tmp, "w");
  if (!f) return false;
  size_t w = f.print(data);
  f.flush();
  f.close();
  if (w != data.length()) {
    FS_IMPL.remove(tmp);
    return false;
  }
  // Rimuovi vecchio e rinomina
  FS_IMPL.remove(path);
  return FS_IMPL.rename(tmp, path);
}

bool mountFS(bool formatOnFail) {
#if USE_LITTLEFS
  if (FS_IMPL.begin(formatOnFail)) return true;
  if (formatOnFail) return FS_IMPL.begin(true);
  return false;
#else
  if (FS_IMPL.begin(formatOnFail)) return true;
  if (formatOnFail) return FS_IMPL.begin(true);
  return false;
#endif
}

// --- Default ---
Config defaultConfig() {
  Config c;
  c.wifi_ssid = "";
  c.wifi_password = "";

  c.mqtt_broker = "mqtt";     // in Docker: hostname del servizio
  c.mqtt_port   = 1883;
  c.mqtt_username = "";
  c.mqtt_password = "";

  c.sensor_pin = 32;
  c.pump_pin   = 26;
  c.relay_pin  = 27;
  c.battery_pin = 34;

  c.moisture_threshold = 25;
  c.pump_duration = 5;
  c.measurement_interval = 1800000L; // 30 min

  c.debug = false;
  c.use_pump = false;
  c.sleep_hours = 0;

  c.use_dhcp = true;
  c.ip_address = "";
  c.gateway    = "";
  c.subnet     = "";

  return c;
}

// --- JSON <-> Config ---
String configToJson(const Config& cfg) {
  StaticJsonDocument<1024> doc;
  doc["wifi_ssid"]      = cfg.wifi_ssid;
  doc["wifi_password"]  = cfg.wifi_password;

  doc["mqtt_broker"]    = cfg.mqtt_broker;
  doc["mqtt_port"]      = cfg.mqtt_port;
  doc["mqtt_username"]  = cfg.mqtt_username;
  doc["mqtt_password"]  = cfg.mqtt_password;

  doc["sensor_pin"]     = cfg.sensor_pin;
  doc["pump_pin"]       = cfg.pump_pin;
  doc["relay_pin"]      = cfg.relay_pin;
  doc["battery_pin"]    = cfg.battery_pin;

  doc["moisture_threshold"]   = cfg.moisture_threshold;
  doc["pump_duration"]        = cfg.pump_duration;
  doc["measurement_interval"] = cfg.measurement_interval;

  doc["debug"]          = cfg.debug;
  doc["use_pump"]       = cfg.use_pump;
  doc["sleep_hours"]    = cfg.sleep_hours;

  doc["use_dhcp"]       = cfg.use_dhcp;
  doc["ip_address"]     = cfg.ip_address;
  doc["gateway"]        = cfg.gateway;
  doc["subnet"]         = cfg.subnet;

  String out;
  serializeJson(doc, out);
  return out;
}

bool jsonToConfig(const String& json, Config& out) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  auto pickStr = [&](const char* k, String& t) {
    if (doc.containsKey(k)) t = doc[k].as<String>();
  };
  auto pickInt = [&](const char* k, int& t) {
    if (doc.containsKey(k)) t = doc[k].as<int>();
  };
  auto pickLong = [&](const char* k, long& t) {
    if (doc.containsKey(k)) t = doc[k].as<long>();
  };
  auto pickBool = [&](const char* k, bool& t) {
    if (doc.containsKey(k)) t = doc[k].as<bool>();
  };

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
  pickInt("pump_duration",      out.pump_duration);
  pickLong("measurement_interval", out.measurement_interval);

  pickBool("debug", out.debug);
  pickBool("use_pump", out.use_pump);
  pickInt("sleep_hours", out.sleep_hours);

  pickBool("use_dhcp", out.use_dhcp);
  pickStr("ip_address", out.ip_address);
  pickStr("gateway", out.gateway);
  pickStr("subnet",  out.subnet);

  return true;
}

// --- Load/Save ---
bool loadConfig(Config& out) {
  if (!FS_IMPL.exists(CONFIG_PATH)) {
    out = defaultConfig();
    return false; // non esiste, torna default
  }
  File f = FS_IMPL.open(CONFIG_PATH, "r");
  if (!f) {
    out = defaultConfig();
    return false;
  }
  String json = f.readString();
  f.close();
  out = defaultConfig();
  bool ok = jsonToConfig(json, out);
  if (!ok) out = defaultConfig();
  return ok;
}

bool saveConfig(const Config& cfg) {
  String json = configToJson(cfg);
  return writeFileAtomic(CONFIG_PATH, json);
}

// Confronto campi MQTT utili (per capire se serve reconnect/reboot)
bool configEqualsMqtt(const Config& a, const Config& b) {
  return a.mqtt_broker == b.mqtt_broker &&
         a.mqtt_port   == b.mqtt_port &&
         a.mqtt_username == b.mqtt_username &&
         a.mqtt_password == b.mqtt_password;
}
