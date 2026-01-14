#include "mirror_serial.h" 
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
#include "telnet_logger.h"
#include "pump_controller.h"

#include "update/UpdateManager.h"
#include "update/FirmwareUpdateStrategy.h"

extern "C" {
  #include "esp_ota_ops.h"
  #include "esp_task_wdt.h"
  #include "esp_system.h"
  #include "esp_sleep.h"
}

// ----------------- Variabili globali -----------------
Config config;

WiFiClient *plainClient = nullptr;
WiFiClientSecure *secureClient = nullptr;

extern String deviceId;

static UpdateManager updater;
static FirmwareUpdateStrategy *fwStrategy = nullptr;

PumpController* pumpController = nullptr;

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool pumpStateAfterWakeup = false;
RTC_DATA_ATTR unsigned long lastWakeupMs = 0;
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

// Convert IANA timezone names to POSIX TZ format
static String ianaToPosixTz(const String& tzName) {
  String tz = tzName;
  tz.toLowerCase();
  tz.trim();
  
  // Common IANA timezone mappings to POSIX TZ format
  if (tz == "europe/rome" || tz == "europe/berlin" || tz == "europe/paris" || 
      tz == "europe/madrid" || tz == "europe/amsterdam" || tz == "europe/brussels") {
    return "CET-1CEST,M3.5.0/2,M10.5.0/3";  // Central European Time
  }
  if (tz == "europe/london" || tz == "europe/dublin") {
    return "GMT0BST,M3.5.0/1,M10.5.0/2";  // British Time
  }
  if (tz == "europe/moscow") {
    return "MSK-3";  // Moscow Standard Time (no DST)
  }
  if (tz == "america/new_york" || tz == "america/toronto") {
    return "EST5EDT,M3.2.0,M11.1.0";  // Eastern Time
  }
  if (tz == "america/chicago" || tz == "america/mexico_city") {
    return "CST6CDT,M3.2.0,M11.1.0";  // Central Time
  }
  if (tz == "america/los_angeles" || tz == "america/vancouver") {
    return "PST8PDT,M3.2.0,M11.1.0";  // Pacific Time
  }
  if (tz == "asia/tokyo") {
    return "JST-9";  // Japan Standard Time (no DST)
  }
  if (tz == "asia/shanghai" || tz == "asia/hong_kong") {
    return "CST-8";  // China Standard Time (no DST)
  }
  if (tz == "utc" || tz == "gmt") {
    return "UTC0";  // Coordinated Universal Time
  }
  
  // If not recognized, assume it's already in POSIX format
  return tzName;
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

  // Use timezone from config, fallback to Europe/Rome if not set or empty
  String tz = config.timezone;
  tz.trim();  // Remove leading/trailing whitespace
  if (tz.length() == 0) {
    tz = "Europe/Rome";  // Default: Italy timezone
    debugLog("TIMEZONE: using fallback " + tz);
  }
  
  // Convert IANA timezone names (e.g., "Europe/Rome") to POSIX format
  String posixTz = ianaToPosixTz(tz);
  if (posixTz != tz) {
    debugLog("TIMEZONE: converted " + tz + " -> " + posixTz);
  } else {
    debugLog("TIMEZONE: using " + tz + " (POSIX format)");
  }
  
  setenv("TZ", posixTz.c_str(), 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  if (timeIsValidLocal()) debugLog("TIME: ok");
  else debugLog("TIME: timeout");
}

// ----------------- Sensore -----------------
int readSoil() {
  // Sample 5 times with 50ms delay to reduce noise
  int samples[5];
  for (int i = 0; i < 5; i++) {
    samples[i] = analogRead(config.sensor_pin);
    delay(50);
  }
  
  // Sort samples for outlier rejection
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 5; j++) {
      if (samples[i] > samples[j]) {
        int temp = samples[i];
        samples[i] = samples[j];
        samples[j] = temp;
      }
    }
  }
  
  // Use median-of-3 (discard min and max outliers)
  int raw = (samples[1] + samples[2] + samples[3]) / 3;
  int perc = map(raw, 4095, 0, 0, 100);
  soilValue = raw;
  soilPercent = perc;
  debugLog("SOIL=" + String(perc) + "% (avg of 5 samples)");
  return perc;
}

// ----------------- Pompa -----------------
void turnOnPump() {
  if (!pumpController) return;
  debugLog("PUMP: ON");
  pumpController->turnOn();

  publishMqtt("bonsai/" + deviceId + "/status/pump", "on", true);

  char buf[32];
  unsigned long long ms = epochMs();
  if (ms > 0) {
    snprintf(buf, sizeof(buf), "%llu", ms);
    publishMqtt("bonsai/" + deviceId + "/status/last_on", buf, true);
  }
}

void turnOffPump() {
  if (!pumpController) return;
  debugLog("PUMP: OFF");
  pumpController->turnOff();
  publishMqtt("bonsai/" + deviceId + "/status/pump", "off", true);
}

// =======================================================
// ======================== SETUP ========================
// =======================================================

// =======================================================
// ======================== SETUP ========================
// =======================================================

unsigned long setupDoneTime = 0;

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
    // Continue anyway with defaults?
  } else {
    debugLog("CONFIG: loaded");
  }

  setup_wifi();
  
  // Check if wakeup from deep sleep and restore state
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    debugLog("WAKEUP: from deep sleep");
    lastWakeupMs = millis();
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    // First boot or reset, not from deep sleep
    debugLog("BOOT: first boot or reset");
    pumpStateAfterWakeup = false;  // Reset to OFF on first boot
  }
  
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
  fwStrategy = new FirmwareUpdateStrategy();
  updater.registerStrategy(fwStrategy);
  updater.runAll();
  debugLog("UPDATER: done");

  pinMode(config.led_pin, OUTPUT);
  pinMode(config.sensor_pin, INPUT);
  
  // Initialize pump controller with 60-second max runtime failsafe
  pumpController = new PumpController(config.pump_pin, 60000);
  pumpController->begin();
  
  // Restore pump state if wakeup from deep sleep
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER && pumpController) {
    debugLog("PUMP: restoring state " + String(pumpStateAfterWakeup ? "ON" : "OFF"));
    pumpController->setState(pumpStateAfterWakeup);
  }

  ArduinoOTA.begin();
  
  // Setup webserver solo se abilitato nel config
  if (config.enable_webserver) {
    setup_webserver(config.pump_pin);
    setupConfigApi();  // API config via HTTP
    debugLog("WEBSERVER: started");
  } else {
    debugLog("WEBSERVER: disabled (enable_webserver=false)");
  }

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

  setupDoneTime = millis();
  debugLog("SETUP: complete. Loop timeout: " + String(config.webserver_timeout));
}

// =======================================================
// ========================= LOOP ========================
// =======================================================

void loop() {
  ArduinoOTA.handle();
  
  // Monitor pump runtime for safety failsafe
  if (pumpController) {
    pumpController->loop();
    
    // Publish emergency alert if pump exceeded max runtime
    if (pumpController->isEmergencyStop() && mqttReady) {
      String alertTopic = "bonsai/" + deviceId + "/alert/pump";
      mqttClient.publish(alertTopic.c_str(), "EMERGENCY_STOP", true);
      debugLog("[PUMP] Published emergency stop alert");
    }
  }
  
  if (mqttReady) {
    loopMqtt();
  }
  loopTelnetLogger();
  esp_task_wdt_reset();

  // Deep sleep management: garantisce almeno un ciclo completo di loop() prima di sleep
  if (!config.debug) {
    unsigned long elapsed = millis() - setupDoneTime;
    unsigned long timeoutMs = 0;
    
    if (config.webserver_timeout > 0) {
      // Timeout esplicito: usa il valore configurato
      timeoutMs = (unsigned long)config.webserver_timeout * 1000;
    } else {
      // Timeout 0 o non impostato: usa minimo 2 secondi per garantire almeno un ciclo
      timeoutMs = 2000;  // Minimo 2 secondi per inizializzazione servizi
    }
    
    if (elapsed >= timeoutMs) {
      debugLog("SLEEP: timeout reached (" + String(elapsed) + "ms)");
      
      // Save pump state before sleep
      if (pumpController) {
        pumpStateAfterWakeup = pumpController->getState();
        debugLog("PUMP: saving state " + String(pumpStateAfterWakeup ? "ON" : "OFF") + " before sleep");
      }
      
      esp_sleep_enable_timer_wakeup(config.sleep_hours * 3600ULL * 1000000ULL);
      delay(100);
      esp_deep_sleep_start();
    }
  }
}
