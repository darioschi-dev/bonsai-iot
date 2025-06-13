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

RTC_DATA_ATTR int bootCount = 0;
Preferences preferences;
Config config;

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
  Serial.printf("[📦] Firmware version: %s\n", FIRMWARE_VERSION);
  Serial.printf("[📦] Boot count: %s\n", String(bootCount));

  if (!loadConfig(config)) {
    Serial.println("[✖] Failed to load config.json");
    return;
  }
  Serial.println("[✔] Config loaded successfully!");

  pinMode(config.led_pin, OUTPUT);
  pinMode(config.sensor_pin, INPUT);
  pinMode(config.pump_pin, OUTPUT);
  digitalWrite(config.pump_pin, HIGH);

  setup_wifi();
  ArduinoOTA.begin();
  setup_webserver(config.pump_pin);

  ++bootCount;
  Serial.printf("Boot count: %d\n", bootCount);

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
