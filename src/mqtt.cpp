#include "mqtt.h"

unsigned long lastMqttPublish = 0;
const unsigned long mqttInterval = 15000; // 15s

String deviceId = "";

// Variabili globali definite qui (dichiarate in mqtt.h)
extern Config config;
extern int soilValue;
extern int soilPercent;

extern WiFiClient* plainClient;
extern WiFiClientSecure* secureClient;
PubSubClient mqttClient;   // DEFINIZIONE UNICA

// =======================================================
// =================== CONFIG MANAGEMENT =================
// =======================================================

bool isNewerConfigVersion(const String& incoming, const String& current)
{
  if (incoming.isEmpty()) return false;
  if (current.isEmpty()) return true;
  return incoming != current && incoming > current;
}

bool applyConfigJson(const String& json)
{
  StaticJsonDocument<1024> doc;
  auto err = deserializeJson(doc, json);
  if (err)
  {
    Serial.printf("[CONFIG] Errore parsing JSON: %s\n", err.c_str());
    return false;
  }

  if (doc.containsKey("wifi_ssid")) config.wifi_ssid = doc["wifi_ssid"].as<String>();
  if (doc.containsKey("wifi_password")) config.wifi_password = doc["wifi_password"].as<String>();
  if (doc.containsKey("mqtt_broker")) config.mqtt_broker = doc["mqtt_broker"].as<String>();
  if (doc.containsKey("mqtt_port")) config.mqtt_port = doc["mqtt_port"].as<int>();
  if (doc.containsKey("mqtt_username")) config.mqtt_username = doc["mqtt_username"].as<String>();
  if (doc.containsKey("mqtt_password")) config.mqtt_password = doc["mqtt_password"].as<String>();
  if (doc.containsKey("sensor_pin")) config.sensor_pin = doc["sensor_pin"].as<int>();
  if (doc.containsKey("pump_pin")) config.pump_pin = doc["pump_pin"].as<int>();
  if (doc.containsKey("relay_pin")) config.relay_pin = doc["relay_pin"].as<int>();
  if (doc.containsKey("battery_pin")) config.battery_pin = doc["battery_pin"].as<int>();
  if (doc.containsKey("moisture_threshold")) config.moisture_threshold = doc["moisture_threshold"].as<int>();
  if (doc.containsKey("pump_duration")) config.pump_duration = doc["pump_duration"].as<int>();
  if (doc.containsKey("measurement_interval")) config.measurement_interval = doc["measurement_interval"].as<int>();
  if (doc.containsKey("use_pump")) config.use_pump = doc["use_pump"].as<bool>();
  if (doc.containsKey("debug")) config.debug = doc["debug"].as<bool>();
  if (doc.containsKey("sleep_hours")) config.sleep_hours = doc["sleep_hours"].as<int>();
  if (doc.containsKey("use_dhcp")) config.use_dhcp = doc["use_dhcp"].as<bool>();
  if (doc.containsKey("ip_address")) config.ip_address = doc["ip_address"].as<String>();
  if (doc.containsKey("gateway")) config.gateway = doc["gateway"].as<String>();
  if (doc.containsKey("subnet")) config.subnet = doc["subnet"].as<String>();
  if (doc.containsKey("ota_manifest_url")) config.ota_manifest_url = doc["ota_manifest_url"].as<String>();
  if (doc.containsKey("update_server")) config.update_server = doc["update_server"].as<String>();
  if (doc.containsKey("config_version")) config.config_version = doc["config_version"].as<String>();

  if (!saveConfig(config))
  {
    Serial.println("[CONFIG] Salvataggio fallito");
    return false;
  }
  return true;
}

// =======================================================
// ================= CONFIG SNAPSHOT PUB =================
// =======================================================

void publishConfigSnapshot()
{
  StaticJsonDocument<1024> doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["mqtt_broker"] = config.mqtt_broker;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_username"] = config.mqtt_username;
  doc["mqtt_password"] = config.mqtt_password;
  doc["sensor_pin"] = config.sensor_pin;
  doc["pump_pin"] = config.pump_pin;
  doc["relay_pin"] = config.relay_pin;
  doc["battery_pin"] = config.battery_pin;
  doc["moisture_threshold"] = config.moisture_threshold;
  doc["pump_duration"] = config.pump_duration;
  doc["measurement_interval"] = config.measurement_interval;
  doc["use_pump"] = config.use_pump;
  doc["debug"] = config.debug;
  doc["sleep_hours"] = config.sleep_hours;
  doc["use_dhcp"] = config.use_dhcp;
  doc["ip_address"] = config.ip_address;
  doc["gateway"] = config.gateway;
  doc["subnet"] = config.subnet;
  doc["ota_manifest_url"] = config.ota_manifest_url;
  doc["update_server"] = config.update_server;
  doc["config_version"] = config.config_version;
  doc["device_id"] = deviceId;

  String out;
  serializeJson(doc, out);

  const String topic = "bonsai/" + deviceId + "/config";
  publishMqtt(topic, out, true);
}

// =======================================================
// ===================== MQTT CALLBACK ===================
// =======================================================

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++)
    message += (char)payload[i];

  if (config.debug)
    Serial.printf("[MQTT] Topic: %s | Msg: %s\n", topic, message.c_str());

  const String t(topic);
  String msg = message;
  msg.trim();

  String msgLower = msg;
  msgLower.toLowerCase();

  if (msg.startsWith("{"))
  {
    StaticJsonDocument<128> j;
    DeserializationError je = deserializeJson(j, msg);
    if (!je && j.containsKey("pump"))
      msgLower = String(j["pump"].as<const char *>());
  }

  if (t == "bonsai/config/set" || t == ("bonsai/" + deviceId + "/config/set"))
  {
    const bool ok = applyConfigJson(msg);
    publishMqtt("bonsai/" + deviceId + "/config/ack", ok ? "{\"ok\":true}" : "{\"ok\":false}");
    if (ok)
    {
      publishConfigSnapshot();
      delay(300);
      ESP.restart();
    }
    return;
  }

  if (t == "bonsai/config" || t == ("bonsai/" + deviceId + "/config"))
  {
    StaticJsonDocument<256> j;
    if (deserializeJson(j, msg) == DeserializationError::Ok)
    {
      const String incomingVer = j["config_version"] | "";
      if (isNewerConfigVersion(incomingVer, config.config_version))
      {
        const bool ok = applyConfigJson(msg);
        publishMqtt("bonsai/" + deviceId + "/config/ack", ok ? "{\"ok\":true}" : "{\"ok\":false}");
        if (ok)
        {
          publishConfigSnapshot();
          delay(300);
          ESP.restart();
        }
      }
    }
    return;
  }

  if (t == ("bonsai/" + deviceId + "/command/pump"))
  {
    const bool on = (msgLower == "on");
    const bool off = (msgLower == "off");
    if (!on && !off) return;

    const String topicBase = "bonsai/" + deviceId + "/status/";

    if (on)
    {
      digitalWrite(config.pump_pin, LOW);
      publishMqtt(topicBase + "pump", "on", true);
      const unsigned long long ms = epochMs();
      if (ms > 0)
      {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", ms);
        publishMqtt(topicBase + "last_on", buf, true);
      }
    }
    else
    {
      digitalWrite(config.pump_pin, HIGH);
      publishMqtt(topicBase + "pump", "off", true);
    }
    return;
  }

  if (t == ("bonsai/" + deviceId + "/command/reboot") ||
      t == ("bonsai/" + deviceId + "/command/restart"))
  {
    ESP.restart();
    return;
  }

  if (t == ("bonsai/" + deviceId + "/command/ota") ||
      t == ("bonsai/ota/force/" + deviceId))
  {
    extern void triggerFirmwareCheck();
    triggerFirmwareCheck();
    return;
  }

  handleMqttConfigCommands(topic, payload, length);
}

// =======================================================
// ===================== CONNECTION ======================
// =======================================================

void connectMqtt()
{
  mqttClient.setServer(config.mqtt_broker.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected())
  {
    Serial.printf("[MQTT] Connessione a %s:%d...\n",
                  config.mqtt_broker.c_str(),
                  config.mqtt_port);

    bool ok = mqttClient.connect(
        deviceId.c_str(),
        config.mqtt_username.c_str(),
        config.mqtt_password.c_str(),
        ("bonsai/" + deviceId + "/status/online").c_str(),
        1,
        true,
        "0");

    if (ok)
    {
      Serial.println("✅ MQTT connesso!");

      const String base = "bonsai/" + deviceId + "/";

      publishMqtt(base + "status/online", "1", true);
      publishMqtt(base + "status/device_id", deviceId, true);

      mqttClient.subscribe((base + "command/pump").c_str());
      mqttClient.subscribe((base + "command/reboot").c_str());
      mqttClient.subscribe((base + "command/restart").c_str());
      mqttClient.subscribe((base + "command/ota").c_str());
      mqttClient.subscribe((base + "config").c_str());
      mqttClient.subscribe((base + "config/set").c_str());
      mqttClient.subscribe(("bonsai/ota/force/" + deviceId).c_str());

      mqttClient.subscribe("bonsai/config");
      mqttClient.subscribe("bonsai/config/set");

      publishConfigSnapshot();
    }
    else
    {
      Serial.print("❌ Fallita. Stato: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

// =======================================================
// ======================== LOOP =========================
// =======================================================

void loopMqtt()
{
  if (!mqttClient.connected())
    connectMqtt();

  mqttClient.loop();

  const unsigned long now = millis();
  if (now - lastMqttPublish > mqttInterval)
  {
    lastMqttPublish = now;
    const String base = "bonsai/" + deviceId + "/status/";

    if (timeIsValid())
      publishMqtt(base + "last_seen", String((long long)epochMs()), true);

    publishMqtt(base + "wifi", String(WiFi.RSSI()), false);

#ifdef FIRMWARE_VERSION
    publishMqtt(base + "firmware", FIRMWARE_VERSION, true);
#endif

    publishMqtt(base + "humidity", String(soilPercent), false);
    publishMqtt(base + "temp", "0", false);
    publishMqtt(base + "battery", String(analogRead(config.battery_pin)), false);

    const bool pumpState = (digitalRead(config.pump_pin) == LOW);
    publishMqtt(base + "pump", pumpState ? "on" : "off", true);
  }
}

// =======================================================
// ======================== SETUP ========================
// =======================================================

void setupMqtt()
{
  setupDeviceId();

  if (config.mqtt_port == 1883)
  {
    plainClient = new WiFiClient();
    mqttClient.setClient(*plainClient);
  }
  else
  {
    secureClient = new WiFiClientSecure();
    secureClient->setInsecure();
    mqttClient.setClient(*secureClient);
  }

  mqttClient.setCallback(mqttCallback);
  connectMqtt();
}
