#include "MirrorSerial.h" 
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <time.h>

#include "config.h"
#include "mail.h"
#include "webserver.h"
#include "mqtt.h"
#include "config_api.h"
#include "logger.h"
#include "TelnetLogger.h"

#include "update/UpdateManager.h"
#include "update/FirmwareUpdateStrategy.h"
#include "update/ConfigUpdateStrategy.h"

extern "C" {
  #include "esp_ota_ops.h"
  #include "esp_task_wdt.h"
  #include "esp_system.h"
}

// ----------------- Variabili globali -----------------
Config config;

WiFiClient *plainClient = nullptr;
WiFiClientSecure *secureClient = nullptr;

extern String deviceId;

static UpdateManager updater;
static FirmwareUpdateStrategy *fwStrategy = nullptr;

RTC_DATA_ATTR int bootCount = 0;
Preferences prefs;

int soilValue = 0;
int soilPercent = 0;

static const char* SYSLOG_HOST = "192.168.1.10";
static const uint16_t SYSLOG_PORT = 5140;
static const char* SYSLOG_APP = "bonsai-esp32";
static const char* SYSLOG_HOSTNAME = "bonsai-esp32";

// ----------------- Debug MQTT helper -----------------
static void debugLog(const String& msg) {
  if (!mqttReady) return;
  if (deviceId.length() == 0) {
    publishMqtt("bonsai/debug", msg, false);
  } else {
    publishMqtt("bonsai/" + deviceId + "/debug", msg, false);
  }
}

// ----------------- Utility -----------------
static String currentAppVersion() {
#ifdef FIRMWARE_VERSION
  return String(FIRMWARE_VERSION);
#else
  const esp_app_desc_t *app = esp_ota_get_app_description();
  if (app && app->version[0]) return String(app->version);
  return "unknown";
#endif
}

// ----------------- OTA da MQTT -----------------
void otaCheckNow() {
  if (!fwStrategy) return;
  if (fwStrategy->checkForUpdate()) {
    if (fwStrategy->performUpdate()) {
      debugLog("OTA: OK");
      LOGI("OTA update done");
    } else {
      debugLog("OTA: FAIL");
      LOGE("OTA update failed");
    }
  } else {
    debugLog("OTA: no update");
    LOGI("No OTA available");
  }
}

// ----------------- Time sync -----------------
static bool timeIsValidLocal(unsigned long timeoutMs = 10000) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (timeIsValid()) return true;
    delay(200);
  }
  return false;
}

// ----------------- WiFi -----------------
void setup_wifi() {
  debugLog("WIFI: starting");
  Serial.print("Connecting to WiFi: ");
  Serial.println(config.wifi_ssid);

  WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
  debugLog("WIFI: connected");

  Logger::begin(SYSLOG_HOST, SYSLOG_PORT, SYSLOG_HOSTNAME, SYSLOG_APP, LOG_INFO);

  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  if (timeIsValidLocal()) debugLog("TIME: ok");
  else debugLog("TIME: timeout");
}

// ----------------- Sensore -----------------
int readSoil() {
  int raw = analogRead(config.sensor_pin);
  int perc = map(raw, 4095, 0, 0, 100);
  soilValue = raw;
  soilPercent = perc;
  debugLog("SOIL=" + String(perc));
  return perc;
}

// ----------------- Pompa -----------------
void turnOnPump() {
  debugLog("PUMP: ON");
  digitalWrite(config.pump_pin, LOW);

  publishMqtt("bonsai/" + deviceId + "/status/pump", "on", true);

  char buf[32];
  unsigned long long ms = epochMs();
  if (ms > 0) {
    snprintf(buf, sizeof(buf), "%llu", ms);
    publishMqtt("bonsai/" + deviceId + "/status/last_on", buf, true);
  }
}

void turnOffPump() {
  debugLog("PUMP: OFF");
  digitalWrite(config.pump_pin, HIGH);
  publishMqtt("bonsai/" + deviceId + "/status/pump", "off", true);
}

// =======================================================
// ======================== SETUP ========================
// =======================================================

void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);
  delay(100);

  setupDeviceId();
  esp_ota_mark_app_valid_cancel_rollback();
  debugLog("FW=" + currentAppVersion());

  if (prefs.begin("bonsai", false)) {
    if (!prefs.isKey("fw_ver")) {
      prefs.putString("fw_ver", currentAppVersion());
    }
    prefs.end();
  }

  ++bootCount;
  debugLog("BOOTCOUNT=" + String(bootCount));

  if (!loadConfig(config)) {
    debugLog("CONFIG: load FAIL");
    return;
  }
  debugLog("CONFIG: loaded");

  setup_wifi();
  
  setupTelnetLogger("bonsai-esp32", 23);

  esp_task_wdt_init(8, true);
  esp_task_wdt_add(NULL);

  setupMqtt();
  debugLog("MQTT: connect start");

  if(mqttReady) {
    publishMqtt("bonsai/debug", "BOOT start", false);
  }

  debugLog("MQTT: connected");
  debugLog("DEVICEID=" + deviceId);

  Logger::enableMqtt(true);
  Logger::setMqttPublish([](const char* topic, const char* payload, bool retain){
    publishMqtt(topic, payload, retain);
  });

  debugLog("UPDATER: run");
  // -------------------------------------------------------
  // OTA FIRMWARE + CONFIG (strategia unica coordinata)
  // -------------------------------------------------------
  fwStrategy = new FirmwareUpdateStrategy();        // UNA SOLA istanza condivisa
  auto* cfgStrategy = new ConfigUpdateStrategy(fwStrategy);

  updater.registerStrategy(fwStrategy);             // 1. firmware
  updater.registerStrategy(cfgStrategy);            // 2. config

  updater.runAll();                                 // esegue eventuali update
  debugLog("UPDATER: done");

  pinMode(config.led_pin, OUTPUT);
  pinMode(config.sensor_pin, INPUT);
  pinMode(config.pump_pin, OUTPUT);
  digitalWrite(config.pump_pin, HIGH);

  ArduinoOTA.begin();
  setup_webserver(config.pump_pin);
  debugLog("WEBSERVER: started");

  int perc = readSoil();
  if (perc < config.moisture_threshold) {
    debugLog("SOIL: dry");
    if (config.use_pump) {
      turnOnPump();
      delay(config.pump_duration * 1000);
      turnOffPump();
    }
  } else {
    debugLog("SOIL: ok");
  }

  if (!config.debug) {
    debugLog("SLEEP: start");
    esp_sleep_enable_timer_wakeup(config.sleep_hours * 3600ULL * 1000000ULL);
    delay(200);
    esp_deep_sleep_start();
  }

  debugLog("SETUP: complete");
}

// =======================================================
// ========================= LOOP ========================
// =======================================================

void loop() {
  ArduinoOTA.handle();
  if (!mqttReady) return;
  loopMqtt();
  loopTelnetLogger();
  esp_task_wdt_reset();
}
