# Bonsai IoT - Firmware Improvements Roadmap

## 🔒 Security & Reliability

### 1. **Watchdog Timer Enhancement**
**Problem**: Current watchdog may not catch all hang scenarios
**Solution**:
```cpp
// Add task-level watchdog with health reporting
esp_task_wdt_add(NULL);
esp_task_wdt_reset();

// Report health via MQTT every loop iteration
void reportHealth() {
  publishMqtt("bonsai/" + deviceId + "/health/watchdog", String(millis()));
}
```

### 2. **Encrypted MQTT (TLS)**
**Current**: Plain MQTT (port 1883)
**Improvement**: Use TLS on port 8883
```cpp
WiFiClientSecure secureClient;
secureClient.setInsecure(); // or load CA cert
mqttClient.setClient(secureClient);
mqttClient.setServer(MQTT_HOST, 8883);
```

### 3. **Brown-out Detection**
**Add**: Low battery warning before shutdown
```cpp
if (batteryPercent < 15) {
  publishMqtt("bonsai/" + deviceId + "/alert/battery", "LOW", true);
  // Enter deep sleep until charged
}
```

---

## 📊 Data Quality

### 4. **Sensor Averaging**
**Problem**: Single readings can be noisy
**Solution**: Sample 5 times, discard outliers, average
```cpp
int readSoilAverage() {
  int samples[5];
  for(int i=0; i<5; i++) {
    samples[i] = analogRead(SOIL_PIN);
    delay(50);
  }
  std::sort(samples, samples+5);
  return (samples[1] + samples[2] + samples[3]) / 3; // median of 3
}
```

### 5. **Timestamp All Messages**
**Add**: Millisecond precision timestamps
```cpp
void publishTelemetry() {
  JsonDocument doc;
  doc["humidity"] = soilPercent;
  doc["temp"] = temperature;
  doc["timestamp_ms"] = epochMs();
  doc["boot_count"] = bootCount;
  publishJson("bonsai/" + deviceId + "/telemetry", doc);
}
```

---

## 🔋 Power Optimization

### 6. **Dynamic Sleep Duration**
**Current**: Fixed sleep intervals
**Smart**: Sleep longer when soil is wet
```cpp
int calculateSleepSeconds() {
  if (soilPercent > 70) return 3600; // 1 hour
  if (soilPercent > 50) return 1800; // 30 min
  if (soilPercent > 30) return 900;  // 15 min
  return 300; // 5 min (critical dry)
}
```

### 7. **WiFi Power Save**
**Add**: Modem sleep when idle
```cpp
WiFi.setSleep(WIFI_PS_MIN_MODEM); // or WIFI_PS_MAX_MODEM
```

---

## 🚿 Pump Safety

### 8. **Pump Failsafe**
**Critical**: Prevent pump running indefinitely
```cpp
class PumpController {
  unsigned long maxRunMs = 60000; // 1 minute MAX
  unsigned long startMs = 0;
  
  void loop() {
    if (state_ && (millis() - startMs > maxRunMs)) {
      turnOff();
      publishMqtt("bonsai/" + deviceId + "/alert/pump", "EMERGENCY_STOP");
    }
  }
};
```

### 9. **Leak Detection**
**Add**: Check if humidity increases after pump runs
```cpp
int humidityBeforePump = soilPercent;
pumpController->turnOn();
delay(30000); // 30 sec
pumpController->turnOff();
delay(60000); // wait for water absorption
readSensors();
if (soilPercent <= humidityBeforePump + 5) {
  // No change = possible leak or empty tank
  publishMqtt("bonsai/" + deviceId + "/alert/leak", "CHECK_WATER_SYSTEM");
}
```

---

## 🔄 OTA Robustness

### 10. **Rollback on Boot Failure**
**Current**: OTA updates may brick device
**Add**: ESP32 native rollback
```cpp
#include "esp_ota_ops.h"

void setup() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      // First boot after OTA, validate
      if (validateSystem()) {
        esp_ota_mark_app_valid_cancel_rollback();
      } else {
        esp_ota_mark_app_invalid_rollback_and_reboot();
      }
    }
  }
}
```

### 11. **Progressive OTA Rollout**
**Add**: Feature flags for gradual firmware deployment
```cpp
// In config.json
{
  "firmware_channel": "stable", // or "beta", "canary"
  "auto_update": true
}

// Server sends different firmware versions based on channel
```

---

## 📡 Network Resilience

### 12. **WiFi Reconnection Strategy**
**Improve**: Exponential backoff
```cpp
int wifiRetries = 0;
int retryDelay = 1000; // start at 1 sec

void connectWiFi() {
  while (WiFi.status() != WL_CONNECTED && wifiRetries < 10) {
    WiFi.begin(SSID, PASSWORD);
    delay(retryDelay);
    retryDelay = min(retryDelay * 2, 30000); // max 30 sec
    wifiRetries++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    // Enter deep sleep and retry later
    ESP.deepSleep(300e6); // 5 min
  }
}
```

### 13. **MQTT QoS Levels**
**Current**: QoS 0 (fire and forget)
**Add**: QoS 1 for critical data
```cpp
// Telemetry: QoS 0 (ok to lose)
mqttClient.publish("bonsai/telemetry", payload, false);

// Alerts: QoS 1 (must deliver)
mqttClient.publish("bonsai/alert", payload, 1, true);
```

---

## 📈 Diagnostics

### 14. **Crash Reporting**
**Add**: Save crash dumps to SPIFFS
```cpp
#include "esp_core_dump.h"

void logCrash() {
  if (esp_core_dump_image_get() != NULL) {
    // Read crash dump from flash
    // Send to server via MQTT
    publishMqtt("bonsai/" + deviceId + "/crash", dumpData);
  }
}
```

### 15. **Performance Metrics**
**Track**: Loop time, memory usage, WiFi quality
```cpp
void reportMetrics() {
  JsonDocument doc;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["min_free_heap"] = ESP.getMinFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["loop_time_ms"] = loopTime;
  doc["uptime_sec"] = millis() / 1000;
  publishJson("bonsai/" + deviceId + "/metrics", doc);
}
```

---

## Priority: HIGH 🔥
1. **Pump Failsafe** (safety critical)
2. **Watchdog Enhancement** (reliability)
3. **OTA Rollback** (prevent bricks)
4. **Sensor Averaging** (data quality)
5. **Brown-out Detection** (prevent data loss)

## Priority: MEDIUM ⚡
6. Encrypted MQTT
7. Dynamic Sleep
8. Leak Detection
9. WiFi Reconnection Strategy
10. Timestamp All Messages

## Priority: LOW 📝
11. Progressive OTA
12. Crash Reporting
13. Performance Metrics
14. WiFi Power Save
15. MQTT QoS Levels
