#include "mqtt.h"
#include "pump_controller.h"
#include "trigger_firmware_check.h"

unsigned long lastMqttPublish = 0;
const unsigned long mqttInterval = 15000; // 15s

String deviceId = "";

// Variabili globali
extern Config config;
extern int soilValue;
extern int soilPercent;
extern PumpController* pumpController;

bool mqttReady = false;

extern WiFiClient* plainClient;
extern WiFiClientSecure* secureClient;
PubSubClient mqttClient;

// ===================== Reconnect Backoff =====================
static unsigned long nextReconnectAt = 0;
static int reconnectAttempts = 0;
static const unsigned long RECONNECT_BASE_MS = 2000;   // 2s
static const unsigned long RECONNECT_MAX_MS  = 60000;  // 60s

// =======================================================
// =============== CONFIG MANAGEMENT (NEW API) ===========
// =======================================================

bool isNewerConfigVersion(const String& incoming, const String& current)
{
  if (incoming.isEmpty()) return false;
  if (current.isEmpty()) return true;
  return incoming != current && incoming > current;
}

// 🔥 Ora usa SEMPRE la API nuova e sicura
bool applyConfigJson(const String& json)
{
  return applyAndPersistConfigJson(json, true);
}

// =======================================================
// ================= CONFIG SNAPSHOT PUB =================
// =======================================================

void publishConfigSnapshot()
{
  StaticJsonDocument<1024> doc;
  doc["wifi_ssid"]            = config.wifi_ssid;
  doc["wifi_password"]        = config.wifi_password;
  doc["mqtt_broker"]          = config.mqtt_broker;
  doc["mqtt_port"]            = config.mqtt_port;
  doc["mqtt_username"]        = config.mqtt_username;
  doc["mqtt_password"]        = config.mqtt_password;
  doc["sensor_pin"]           = config.sensor_pin;
  doc["pump_pin"]             = config.pump_pin;
  doc["relay_pin"]            = config.relay_pin;
  doc["battery_pin"]          = config.battery_pin;
  doc["moisture_threshold"]   = config.moisture_threshold;
  doc["pump_duration"]        = config.pump_duration;
  doc["measurement_interval"] = config.measurement_interval;
  doc["use_pump"]             = config.use_pump;
  doc["debug"]                = config.debug;
  doc["enable_webserver"]     = config.enable_webserver;
  doc["sleep_hours"]          = config.sleep_hours;
  doc["use_dhcp"]             = config.use_dhcp;
  doc["ip_address"]           = config.ip_address;
  doc["gateway"]              = config.gateway;
  doc["subnet"]               = config.subnet;
  doc["ota_manifest_url"]     = config.ota_manifest_url;
  doc["update_server"]        = config.update_server;
  doc["config_version"]       = config.config_version;
  doc["timezone"]              = config.timezone;
  doc["device_id"]            = deviceId;

  String out;
  serializeJson(doc, out);

  publishMqtt("bonsai/" + deviceId + "/config", out, true);
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

  // ========= PUMP COMMAND ===========
  String msgLower = msg; 
  msgLower.toLowerCase();

  if (msg.startsWith("{"))
  {
    StaticJsonDocument<128> j;
    if (deserializeJson(j, msg) == DeserializationError::Ok && j.containsKey("pump"))
      msgLower = String(j["pump"].as<const char*>());
  }

  // ========= CONFIG SET DIRECT ===========
  if (t == "bonsai/config/set" || t == ("bonsai/" + deviceId + "/config/set"))
  {
    bool ok = applyConfigJson(msg);
    publishMqtt("bonsai/" + deviceId + "/config/ack", ok ? "{\"ok\":true}" : "{\"ok\":false}");

    if (ok)
    {
      publishConfigSnapshot();
      delay(300);
      ESP.restart();
    }
    return;
  }

  // ========= CONFIG VERSIONED UPDATE ===========
  if (t == "bonsai/config" || t == ("bonsai/" + deviceId + "/config"))
  {
    StaticJsonDocument<256> j;
    if (deserializeJson(j, msg) == DeserializationError::Ok)
    {
      const String incomingVer = j["config_version"] | "";
      if (isNewerConfigVersion(incomingVer, config.config_version))
      {
        bool ok = applyConfigJson(msg);
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

  // ========= PUMP CONTROL ===========
  if (t == ("bonsai/" + deviceId + "/command/pump"))
  {
    if (!pumpController) return;
    
    bool on = (msgLower == "on");
    bool off = (msgLower == "off");
    if (!on && !off) return;

    const String base = "bonsai/" + deviceId + "/status/";

    if (on)
    {
      pumpController->turnOn();
      publishMqtt(base + "pump", "on", true);

      unsigned long long ms = epochMs();
      if (ms > 0)
      {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", ms);
        publishMqtt(base + "last_on", buf, true);
      }
    }
    else
    {
      pumpController->turnOff();
      publishMqtt(base + "pump", "off", true);
    }
    return;
  }

  // ========= REBOOT ===========
  if (t == ("bonsai/" + deviceId + "/command/reboot") ||
      t == ("bonsai/" + deviceId + "/command/restart"))
  {
    ESP.restart();
    return;
  }

  // ========= FORCED OTA ===========
  if (t == ("bonsai/" + deviceId + "/command/ota") ||
      t == ("bonsai/ota/force/" + deviceId))
  {
    triggerFirmwareCheck();
    return;
  }

  // ========= CONFIG API COMMANDS (OLD TOPICS) ===========
  handleMqttConfigCommands(topic, payload, length);
}

// =======================================================
// ===================== CONNECTION ======================
// =======================================================

void connectMqtt()
{
  mqttClient.setServer(config.mqtt_broker.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  unsigned long now = millis();
  if (now < nextReconnectAt && !mqttClient.connected()) {
    // Respect backoff window
    return;
  }

  if (mqttClient.connected()) {
    // Already connected; ensure callbacks are set
    mqttClient.setCallback(mqttCallback);
    return;
  }

  Serial.printf("[MQTT] Connessione a %s:%d...\n",
                config.mqtt_broker.c_str(), config.mqtt_port);

  bool ok = mqttClient.connect(
    deviceId.c_str(),
    config.mqtt_username.c_str(),
    config.mqtt_password.c_str(),
    ("bonsai/" + deviceId + "/status/online").c_str(),
    1, true, "0"
  );

  if (ok)
  {
    Serial.println("✅ MQTT connesso!");
    mqttReady = true;
    reconnectAttempts = 0;
    nextReconnectAt = 0;

    String base = "bonsai/" + deviceId + "/";

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

    // Exponential backoff up to 60s
    reconnectAttempts = (reconnectAttempts < 12) ? reconnectAttempts + 1 : reconnectAttempts; // cap shift
    unsigned long delayMs = RECONNECT_BASE_MS << (reconnectAttempts - 1);
    if (delayMs > RECONNECT_MAX_MS) delayMs = RECONNECT_MAX_MS;
    nextReconnectAt = now + delayMs;
    Serial.printf("[MQTT] Next reconnect in %lu ms\n", delayMs);
  }
}

// =======================================================
// ======================== LOOP =========================
// =======================================================

void loopMqtt()
{
  if (!mqttClient.connected()) {
    connectMqtt();
    if (!mqttClient.connected()) {
      // Skip publish work when disconnected
      return;
    }
  }

  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastMqttPublish > mqttInterval)
  {
    lastMqttPublish = now;

    String base = "bonsai/" + deviceId + "/status/";

    if (timeIsValid())
      publishMqtt(base + "last_seen", String((long long)epochMs()), true);

    publishMqtt(base + "wifi", String(WiFi.RSSI()), false);

#ifdef FIRMWARE_VERSION
    publishMqtt(base + "firmware", FIRMWARE_VERSION, true);
#endif

    publishMqtt(base + "humidity", String(soilPercent), false);
    publishMqtt(base + "temp", "0", false);
    publishMqtt(base + "battery", String(analogRead(config.battery_pin)), false);

    if (pumpController) {
      publishMqtt(base + "pump",
        pumpController->getState() ? "on" : "off", true
      );
    }
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
