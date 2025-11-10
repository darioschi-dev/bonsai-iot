#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <time.h> // per NTP/timestamp

#include "config.h"
#include "mail.h"
#include "webserver.h"
#include "mqtt.h"
#include "config_api.h"
#include "logger.h"

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
PubSubClient mqttClient;

static UpdateManager updater;
static FirmwareUpdateStrategy *fwStrategy = nullptr;

//String deviceId = String(ESP.getChipModel());
String deviceId = "";

RTC_DATA_ATTR int bootCount = 0;
Preferences prefs;

int soilValue = 0;
int soilPercent = 0;

// ===== SYSLOG target (cambia IP) =====
static const char* SYSLOG_HOST = "192.168.1.10";
static const uint16_t SYSLOG_PORT = 5140;
static const char* SYSLOG_APP = "bonsai-esp32";
static const char* SYSLOG_HOSTNAME = "bonsai-esp32";

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
      Serial.println("✅ OTA eseguito");
      LOGI("OTA update done");
    } else {
      Serial.println("❌ OTA fallito");
      LOGE("OTA update failed");
    }
  } else {
    if (config.debug) {
      Serial.println("ℹ️ Nessun OTA disponibile");
      LOGI("No OTA available");
    }
  }
}

static bool waitForTime(unsigned long timeoutMs = 10000) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (timeIsValid()) return true; // viene da mqtt.h
    delay(200);
  }
  return false;
}

// ----------------- WiFi/NTP -----------------
void setup_wifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(config.wifi_ssid);

  WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  // Inizializza logger UDP (dopo WiFi)
  Logger::begin(SYSLOG_HOST, SYSLOG_PORT, SYSLOG_HOSTNAME, SYSLOG_APP, LOG_INFO);

  // TZ Europa/Roma (CET/CEST) + NTP
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // attesa minima della sync NTP
  if (waitForTime()) {
    if (config.debug) Serial.println("[TIME] NTP OK");
    LOGI("NTP sync OK");
  } else {
    if (config.debug) Serial.println("[TIME] NTP non sincronizzato entro il timeout");
    LOGW("NTP sync timeout");
  }
}

// ----------------- Sensore -----------------
int readSoil() {
  int raw = analogRead(config.sensor_pin);
  int perc = map(raw, 4095, 0, 0, 100);
  Serial.printf("Soil raw: %d, percentage: %d\n", raw, perc);
  LOGI("Soil raw=%d perc=%d", raw, perc);
  soilValue = raw;
  soilPercent = perc;
  return perc;
}

// ----------------- Pompa -----------------
void turnOnPump() {
  Serial.println("Turning ON pump");
  LOGW("Pump ON for %ds", config.pump_duration);
  digitalWrite(config.pump_pin, LOW);

  // Stato pompa (retained)
  publishMqtt("bonsai/status/pump", "on", true);
  // Timestamp ultima accensione (ms)
  char buf[32];
  unsigned long long ms = epochMs();
  if (ms > 0) {
    snprintf(buf, sizeof(buf), "%llu", ms);
    publishMqtt("bonsai/status/last_on", buf, true);
  }
}

void turnOffPump() {
  Serial.println("Turning OFF pump");
  LOGW("Pump OFF");
  digitalWrite(config.pump_pin, HIGH);

  publishMqtt("bonsai/status/pump", "off", true);
}

// ----------------- Setup -----------------
void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);
  delay(100);
  // segna app valida post-OTA
  esp_ota_mark_app_valid_cancel_rollback();

  Serial.printf("[📦] Firmware version: %s\n", currentAppVersion().c_str());

  if (prefs.begin("bonsai", false)) {
    if (!prefs.isKey("fw_ver")) {
      prefs.putString("fw_ver", currentAppVersion());
    }
    prefs.end();
  }

  ++bootCount;
  Serial.printf("[📦] Boot count: %d\n", bootCount);

  if (!loadConfig(config)) {
    Serial.println("[✖] Failed to load config.json");
    LOGE("Failed to load config.json");
    return;
  }
  Serial.println("[✔] Config loaded successfully!");
  LOGI("Config loaded");

  setup_wifi();

  LOGI("--- BOOT --- ver=%s", currentAppVersion().c_str());
  LOGI("Reset reason=%s", Logger::resetReasonStr(esp_reset_reason()));
  LOGI("WiFi=%s RSSI=%d IP=%s FreeHeap=%u",
       (WiFi.status()==WL_CONNECTED?"OK":"DOWN"),
       WiFi.RSSI(),
       WiFi.localIP().toString().c_str(),
       ESP.getFreeHeap());

  // Watchdog per blocchi silenziosi
  esp_task_wdt_init(8, true);
  esp_task_wdt_add(NULL);

  setupMqtt();
   // Se vuoi anche log minimi via MQTT (WARNING/ERROR), abilita e passa una lambda di publish:
  Logger::enableMqtt(true);
  Logger::setMqttPublish([](const char* topic, const char* payload, bool retain){
    publishMqtt(topic, payload, retain);
  });
  // Aggiornamenti (OTA/config)
  fwStrategy = new FirmwareUpdateStrategy();
  updater.registerStrategy(fwStrategy);
  updater.registerStrategy(new ConfigUpdateStrategy());
  updater.runAll();
  if (config.debug) {
    Serial.println("[🔄] Update check complete");
    LOGI("Update check complete");
  }

  pinMode(config.led_pin, OUTPUT);
  pinMode(config.sensor_pin, INPUT);
  pinMode(config.pump_pin, OUTPUT);
  digitalWrite(config.pump_pin, HIGH); // relè idle (active-LOW)

  ArduinoOTA.begin();
  setup_webserver(config.pump_pin); // ✅ mantenuto
  // Misura e (eventualmente) irriga
  int perc = readSoil();
  if (perc < config.moisture_threshold) {
    Serial.println("Soil dry, condition met");
    LOGI("Soil dry, condition met (perc=%d < thr=%d)", perc, config.moisture_threshold);
    if (config.use_pump) {
      turnOnPump();
      delay(config.pump_duration * 1000);
      turnOffPump();
    }
  } else {
    Serial.println("Soil OK");
    LOGI("Soil OK (perc=%d)", perc);
  }

  if (!config.debug) {
    esp_sleep_enable_timer_wakeup(config.sleep_hours * 3600ULL * 1000000ULL);
    Serial.printf("Sleeping for %d hours\n", config.sleep_hours);
    LOGI("Sleeping for %d hours", config.sleep_hours);
    delay(200);
    esp_deep_sleep_start();
  }
}

// ----------------- Loop -----------------
void loop() {
  ArduinoOTA.handle();
  loopMqtt();
  esp_task_wdt_reset(); // reset watchdog
}
