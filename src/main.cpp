#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <time.h>   // per NTP/timestamp

#include "config.h"
#include "mail.h"
#include "webserver.h"
#include "mqtt.h"
#include "config_api.h"

#include "update/UpdateManager.h"
#include "update/FirmwareUpdateStrategy.h"
#include "update/ConfigUpdateStrategy.h"

extern "C" {
  #include "esp_ota_ops.h"
}

// ----------------- Variabili globali -----------------
Config config;

WiFiClient *plainClient = nullptr;
WiFiClientSecure *secureClient = nullptr;
PubSubClient mqttClient;

static UpdateManager updater;
static FirmwareUpdateStrategy* fwStrategy = nullptr;

String deviceId = String(ESP.getChipModel());

RTC_DATA_ATTR int bootCount = 0;
Preferences prefs;

int soilValue = 0;
int soilPercent = 0;

// ----------------- Utility -----------------
static String currentAppVersion() {
#ifdef FIRMWARE_VERSION
  return String(FIRMWARE_VERSION);
#else
  const esp_app_desc_t* app = esp_ota_get_app_description();
  if (app && app->version[0]) return String(app->version);
  return "unknown";
#endif
}

static unsigned long long epochMs() {
  time_t nowSec;
  if (time(&nowSec) && nowSec > 100000) {
    return (unsigned long long)nowSec * 1000ULL;
  }
  return (unsigned long long)millis();
}

// ----------------- OTA da MQTT -----------------
void otaCheckNow() {
  if (!fwStrategy) return;
  if (fwStrategy->checkForUpdate()) {
    if (fwStrategy->performUpdate()) {
      Serial.println("✅ OTA eseguito");
    } else {
      Serial.println("❌ OTA fallito");
    }
  } else {
    if (config.debug) Serial.println("ℹ️ Nessun OTA disponibile");
  }
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

  // TZ Europa/Roma (CET/CEST) + NTP
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

// ----------------- Sensore -----------------
int readSoil() {
  int raw = analogRead(config.sensor_pin);
  int perc = map(raw, 4095, 0, 0, 100);
  Serial.printf("Soil raw: %d, percentage: %d\n", raw, perc);
  soilValue = raw;
  soilPercent = perc;
  return perc;
}

// ----------------- Pompa -----------------
void turnOnPump() {
  Serial.println("Turning ON pump");
  digitalWrite(config.pump_pin, LOW);

  // Stato pompa (retained)
  publishMqtt("bonsai/status/pump", "on", true);

  // Timestamp ultima accensione (ms)
  char buf[32];
  unsigned long long ms = epochMs();
  snprintf(buf, sizeof(buf), "%llu", ms);
  publishMqtt("bonsai/status/last_on", buf, true);
}

void turnOffPump() {
  Serial.println("Turning OFF pump");
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
    return;
  }
  Serial.println("[✔] Config loaded successfully!");

  setup_wifi();

  // ✅ Connetti MQTT PRIMA di eventuali publish (irrigazione)
  setupMqtt();

  // Aggiornamenti (OTA/config)
  fwStrategy = new FirmwareUpdateStrategy();
  updater.registerStrategy(fwStrategy);
  updater.registerStrategy(new ConfigUpdateStrategy());
  updater.runAll();
  if (config.debug) Serial.println("[🔄] Update check complete");

  pinMode(config.led_pin, OUTPUT);
  pinMode(config.sensor_pin, INPUT);
  pinMode(config.pump_pin, OUTPUT);
  digitalWrite(config.pump_pin, HIGH); // relè idle (active-LOW)

  ArduinoOTA.begin();
  setup_webserver(config.pump_pin);

  // Misura e (eventualmente) irriga
  int perc = readSoil();
  if (perc > config.moisture_threshold) {
    Serial.println("Soil dry, condition met");
    if (config.use_pump) {
      turnOnPump();
      delay(config.pump_duration * 1000);
      turnOffPump();
    }
  } else {
    Serial.println("Soil OK");
  }

  if (!config.debug) {
    esp_sleep_enable_timer_wakeup(config.sleep_hours * 3600ULL * 1000000ULL);
    Serial.printf("Sleeping for %d hours\n", config.sleep_hours);
    delay(100);
    esp_deep_sleep_start();
  }
}

// ----------------- Loop -----------------
void loop() {
  ArduinoOTA.handle();
  loopMqtt();
}
