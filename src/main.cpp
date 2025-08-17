#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "config.h"
#include "mail.h"
#include "webserver.h"
#include "mqtt.h"
#include "version_auto.h"
#include "config_api.h"

#include "update/UpdateManager.h"
#include "update/FirmwareUpdateStrategy.h"
#include "update/ConfigUpdateStrategy.h"

#include "config.h"
extern "C" {
  #include "esp_ota_ops.h"
}

// DEFINIZIONE reale della variabile globale
Config config;

WiFiClient *plainClient = nullptr;
WiFiClientSecure *secureClient = nullptr;
PubSubClient mqttClient; // definizione globale vera

static UpdateManager updater;
static FirmwareUpdateStrategy* fwStrategy = nullptr;

String deviceId = String(ESP.getChipModel());

static String currentAppVersion() {
  const esp_app_desc_t* app = esp_ota_get_app_description();
  if (app && app->version[0] != '\0') return String(app->version);
  return String(FIRMWARE_VERSION); // fallback generato da generate_version.py
}

// chiamata dal callback MQTT (dichiarata extern in mqtt.h)
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


RTC_DATA_ATTR int bootCount = 0;
Preferences preferences;

int soilValue = 0;
int soilPercent = 0;

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
}

int readSoil() {
  int raw = analogRead(config.sensor_pin);
  int perc = map(raw, 4095, 0, 0, 100);
  Serial.printf("Soil raw: %d, percentage: %d\n", raw, perc);
  soilValue = raw;
  soilPercent = perc;
  return perc;
}

void turnOnPump() {
  Serial.println("Turning ON pump");
  digitalWrite(config.pump_pin, LOW);
}

void turnOffPump() {
  Serial.println("Turning OFF pump");
  digitalWrite(config.pump_pin, HIGH);
}

void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);
  delay(100);

  // 🔒 marca valida (importantissimo dopo un OTA riuscito)
  esp_ota_mark_app_valid_cancel_rollback();

  Serial.printf("[📦] Firmware version: %s\n", currentAppVersion().c_str());

  // bootCount è RTC_DATA_ATTR: incrementa PRIMA di stamparlo
  ++bootCount;
  Serial.printf("[📦] Boot count: %d\n", bootCount);

  if (!loadConfig(config)) {
    Serial.println("[✖] Failed to load config.json");
    return;
  }
  Serial.println("[✔] Config loaded successfully!");

  setup_wifi();

  // 🔄 Check aggiornamenti (una volta al boot)
  fwStrategy = new FirmwareUpdateStrategy();
  updater.registerStrategy(fwStrategy);
  updater.registerStrategy(new ConfigUpdateStrategy());
  updater.runAll();
  if (config.debug) Serial.println("[🔄] Update check complete");

  pinMode(config.led_pin, OUTPUT);
  pinMode(config.sensor_pin, INPUT);
  pinMode(config.pump_pin, OUTPUT);
  digitalWrite(config.pump_pin, HIGH);

  ArduinoOTA.begin();
  setup_webserver(config.pump_pin);

  int perc = readSoil();
  if (perc > config.moisture_threshold) {
    Serial.println("Soil dry, condition met");
    if (config.use_pump) {
      turnOnPump();
      delay(3000);
      turnOffPump();
    }
  } else {
    Serial.println("Soil OK");
  }

  setupMqtt();

  if (!config.debug) {
    esp_sleep_enable_timer_wakeup(config.sleep_hours * 3600ULL * 1000000ULL);
    Serial.printf("Sleeping for %d hours\n", config.sleep_hours);
    delay(100);
    esp_deep_sleep_start();
  }
}

void loop() {
  ArduinoOTA.handle();
  loopMqtt();
}
