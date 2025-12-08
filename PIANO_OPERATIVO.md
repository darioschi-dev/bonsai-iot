# 🎯 Piano Operativo - Patch Atomiche

Questo documento descrive le patch da applicare in ordine, ognuna testabile in isolamento.

---

## 📦 FASE 1: Fix Bug Critici

### **PATCH 1.1: Fix Pompa ON/OFF - Centralizzazione Stato**

**Obiettivo:** Eliminare race condition e stato inconsistente della pompa.

**File da creare:**
1. `src/pump_controller.h` - Header classe PumpController
2. `src/pump_controller.cpp` - Implementazione classe PumpController

**File da modificare:**
1. `src/main.cpp` - Usare PumpController invece di funzioni dirette
2. `src/mqtt.cpp` - Usare PumpController nel callback
3. `src/webserver.cpp` - Usare PumpController negli endpoint API
4. `src/config.h` - (opzionale) Aggiungere stato pompa persistente

**Dettagli implementazione:**

#### Step 1.1.1: Creare `src/pump_controller.h`
```cpp
#pragma once
#include <Arduino.h>
#include "config.h"

class PumpController {
private:
    int pumpPin_;
    bool state_;  // true = ON, false = OFF
    unsigned long lastChangeMs_;
    
public:
    PumpController(int pin);
    void begin();
    bool turnOn();
    bool turnOff();
    bool getState() const { return state_; }
    unsigned long getLastChangeMs() const { return lastChangeMs_; }
    void setState(bool on);  // Forza stato (per restore dopo wakeup)
};
```

#### Step 1.1.2: Creare `src/pump_controller.cpp`
```cpp
#include "pump_controller.h"

PumpController::PumpController(int pin) : pumpPin_(pin), state_(false), lastChangeMs_(0) {}

void PumpController::begin() {
    pinMode(pumpPin_, OUTPUT);
    digitalWrite(pumpPin_, HIGH);  // OFF di default (LOW = ON per relay)
    state_ = false;
}

bool PumpController::turnOn() {
    if (state_) return false;  // Già accesa
    digitalWrite(pumpPin_, LOW);
    state_ = true;
    lastChangeMs_ = millis();
    return true;
}

bool PumpController::turnOff() {
    if (!state_) return false;  // Già spenta
    digitalWrite(pumpPin_, HIGH);
    state_ = false;
    lastChangeMs_ = millis();
    return true;
}

void PumpController::setState(bool on) {
    digitalWrite(pumpPin_, on ? LOW : HIGH);
    state_ = on;
    lastChangeMs_ = millis();
}
```

#### Step 1.1.3: Modificare `src/main.cpp`
- Aggiungere `#include "pump_controller.h"`
- Creare istanza globale: `PumpController* pumpController = nullptr;`
- In `setup()`: `pumpController = new PumpController(config.pump_pin); pumpController->begin();`
- Sostituire `turnOnPump()` e `turnOffPump()` con chiamate a `pumpController->turnOn()` e `turnOff()`
- Rimuovere `pinMode(config.pump_pin, OUTPUT)` e `digitalWrite(config.pump_pin, HIGH)` duplicati

#### Step 1.1.4: Modificare `src/mqtt.cpp`
- Aggiungere `extern PumpController* pumpController;`
- Nel callback `mqttCallback()`, sostituire `digitalWrite(config.pump_pin, ...)` con `pumpController->turnOn()` / `turnOff()`

#### Step 1.1.5: Modificare `src/webserver.cpp`
- Aggiungere `extern PumpController* pumpController;`
- Negli endpoint `/api/pump/on` e `/api/pump/off`, usare `pumpController->turnOn()` / `turnOff()`
- In `/api/soil`, leggere stato da `pumpController->getState()`

**Test:**
- [ ] Test pompa ON via HTTP → verifica stato
- [ ] Test pompa OFF via HTTP → verifica stato
- [ ] Test pompa ON via MQTT → verifica stato
- [ ] Test pompa OFF via MQTT → verifica stato
- [ ] Test simultaneo HTTP + MQTT → verifica nessun race condition

---

### **PATCH 1.2: Fix Deep Sleep - Persistenza Stato**

**Obiettivo:** Mantenere stato consistente dopo wakeup da deep sleep.

**File da modificare:**
1. `src/main.cpp` - Salvare/ripristinare stato prima/dopo sleep
2. `src/pump_controller.h/.cpp` - (se PATCH 1.1 applicata) Aggiungere persistenza

**Dettagli implementazione:**

#### Step 1.2.1: Aggiungere variabili RTC_DATA_ATTR in `src/main.cpp`
```cpp
RTC_DATA_ATTR bool pumpStateAfterWakeup = false;
RTC_DATA_ATTR unsigned long lastWakeupMs = 0;
```

#### Step 1.2.2: Modificare `setup()` in `src/main.cpp`
- Dopo `setup_wifi()`, controllare se wakeup da deep sleep:
  ```cpp
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
      // Ripristina stato pompa
      if (pumpController) {
          pumpController->setState(pumpStateAfterWakeup);
      }
      lastWakeupMs = millis();
  }
  ```

#### Step 1.2.3: Prima di `esp_deep_sleep_start()` in `src/main.cpp`
- Salvare stato pompa:
  ```cpp
  if (pumpController) {
      pumpStateAfterWakeup = pumpController->getState();
  }
  ```

#### Step 1.2.4: Ri-inizializzare webserver dopo wakeup
- In `setup()`, dopo wakeup detection, chiamare `setup_webserver()` se necessario

**Test:**
- [ ] Test deep sleep → wakeup → verifica stato pompa ripristinato
- [ ] Test deep sleep → wakeup → verifica webserver funzionante
- [ ] Test deep sleep → wakeup → verifica MQTT riconnesso

---

### **PATCH 1.3: Fix Timestamp/Timezone - Configurabile**

**Obiettivo:** Timezone configurabile e timestamp corretti.

**File da modificare:**
1. `src/config.h` - Aggiungere campo `timezone`
2. `src/main.cpp` - Usare `config.timezone` invece di hardcoded
3. `data/config.example.json` - Aggiungere campo `timezone`
4. `src/config_api.cpp` - Serializzare/deserializzare `timezone`
5. `scripts/generate_version.py` - Includere timezone nel build time

**Dettagli implementazione:**

#### Step 1.3.1: Modificare `src/config.h`
- Aggiungere in struct `Config`:
  ```cpp
  String timezone;  // Es: "CET-1CEST,M3.5.0/2,M10.5.0/3"
  ```

#### Step 1.3.2: Modificare `data/config.example.json`
- Aggiungere:
  ```json
  "timezone": "CET-1CEST,M3.5.0/2,M10.5.0/3"
  ```

#### Step 1.3.3: Modificare `src/config_api.cpp`
- In `configToJson()`, aggiungere: `d["timezone"] = c.timezone;`
- In `jsonToConfig()`, aggiungere: `if (d.containsKey("timezone")) out.timezone = d["timezone"].as<String>();`

#### Step 1.3.4: Modificare `src/main.cpp` in `setup_wifi()`
- Sostituire:
  ```cpp
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  ```
  Con:
  ```cpp
  String tz = config.timezone.length() > 0 ? config.timezone : "CET-1CEST,M3.5.0/2,M10.5.0/3";
  setenv("TZ", tz.c_str(), 1);
  ```

#### Step 1.3.5: Modificare `scripts/generate_version.py`
- Aggiungere timezone-aware build time:
  ```python
  import os
  tz = os.environ.get("TZ", "UTC")
  build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S %Z")
  ```
  Oppure usare timezone dal config se disponibile.

**Test:**
- [ ] Test con timezone diverso (es: "UTC", "EST5EDT")
- [ ] Verifica timestamp MQTT corretti
- [ ] Verifica build time include timezone

---

## 📦 FASE 2: Miglioramenti Dashboard

### **PATCH 2.1: Miglioramento Dashboard - API Dati Storici**

**Obiettivo:** Fornire dati storici per grafici nella dashboard.

**File da creare:**
1. `src/data_logger.h` - Header per logging dati
2. `src/data_logger.cpp` - Implementazione logging su Preferences/SPIFFS

**File da modificare:**
1. `src/webserver.cpp` - Aggiungere endpoint `/api/history` e `/api/stats`
2. `src/main.cpp` - Integrare logging nel loop

**Dettagli implementazione:**

#### Step 2.1.1: Creare `src/data_logger.h`
```cpp
#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct DataPoint {
    unsigned long timestamp;
    int humidity;
    int soilValue;
    bool pumpState;
};

class DataLogger {
private:
    Preferences prefs_;
    static const int MAX_POINTS = 100;  // Ultimi 100 punti
    
public:
    bool begin();
    bool log(int humidity, int soilValue, bool pumpState);
    bool getHistory(DataPoint* out, int maxCount, int& count);
    bool getStats(int& avgHumidity, int& minHumidity, int& maxHumidity);
    void clear();
};
```

#### Step 2.1.2: Creare `src/data_logger.cpp`
- Implementare logging circolare su Preferences
- Implementare `getHistory()` per ultimi N punti
- Implementare `getStats()` per statistiche

#### Step 2.1.3: Modificare `src/webserver.cpp`
- Aggiungere endpoint:
  ```cpp
  server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* req){
      // Restituisce JSON array di DataPoint
  });
  
  server.on("/api/stats", HTTP_GET, [](AsyncWebServerRequest* req){
      // Restituisce statistiche (avg, min, max)
  });
  ```

#### Step 2.1.4: Modificare `src/main.cpp`
- In `loop()`, chiamare `dataLogger.log()` ogni intervallo (es: ogni 15 minuti)

**Test:**
- [ ] Test logging dati
- [ ] Test endpoint `/api/history`
- [ ] Test endpoint `/api/stats`
- [ ] Verifica memoria (Preferences limit)

---

## 📦 FASE 3: Setup OTA Server Integrato

### **PATCH 3.1: Documentazione e Verifica OTA Server**

**Obiettivo:** Verificare e documentare endpoint OTA nel server Node.js.

**Nota:** Richiede accesso al repository Node.js (`bonsai-mqtt-dashboard`).

**File da verificare (nel repo Node.js):**
1. `server.js` o `app.js` - Endpoint principali
2. `routes/firmware.js` - Route OTA
3. `public/firmware/` - Directory storage

**Endpoint da verificare:**
- `POST /firmware/upload` - Upload firmware
- `GET /firmware/manifest.json` - Manifest OTA
- Validazione SHA256
- Storage filesystem

**Documentazione da creare:**
- `OTA_SERVER_SETUP.md` - Guida setup server OTA
- Specifica API endpoint
- Esempi curl per test

---

## 📦 FASE 4: Gestione Config Migliorata

### **PATCH 4.1: Validazione Config lato ESP32**

**Obiettivo:** Validare config.json e fallback a default se corrotto.

**File da creare:**
1. `src/config_validator.h` - Header validazione
2. `src/config_validator.cpp` - Implementazione validazione

**File da modificare:**
1. `src/config_api.cpp` - Usare validatore in `loadConfig()`
2. `src/config.h` - Aggiungere funzione `getDefaultConfig()`

**Dettagli implementazione:**

#### Step 4.1.1: Creare `src/config_validator.h`
```cpp
#pragma once
#include "config.h"

bool validateConfig(const Config& config);
Config getDefaultConfig();
```

#### Step 4.1.2: Creare `src/config_validator.cpp`
- Implementare `validateConfig()` - verifica range pin, valori sensati
- Implementare `getDefaultConfig()` - config di fallback

#### Step 4.1.3: Modificare `src/config_api.cpp`
- In `loadConfig()`, dopo `jsonToConfig()`, chiamare `validateConfig()`
- Se fallisce, usare `getDefaultConfig()` e salvare

**Test:**
- [ ] Test config valido → carica normalmente
- [ ] Test config corrotto → fallback a default
- [ ] Test config con valori fuori range → correzione automatica

---

### **PATCH 4.2: Gestione Config lato Server**

**Obiettivo:** API REST per gestione config remota.

**Nota:** Richiede accesso al repository Node.js.

**File da verificare/modificare (nel repo Node.js):**
1. `routes/config.js` - API config
2. Validazione schema JSON
3. Versioning config

**Endpoint da implementare:**
- `GET /api/config/:deviceId` - Leggi config device
- `POST /api/config/:deviceId` - Aggiorna config
- `GET /api/config/versions/:deviceId` - Storia versioni

---

## 📋 Checklist Esecuzione

### Pre-requisiti
- [ ] Backup repository corrente
- [ ] Creare branch `feature/fixes` per ogni patch
- [ ] Test ambiente: ESP32 reale o Wokwi

### Ordine Esecuzione
1. [ ] **PATCH 1.1** - Fix Pompa (URGENTE)
2. [ ] **PATCH 1.2** - Fix Deep Sleep (URGENTE)
3. [ ] **PATCH 1.3** - Fix Timestamp (ALTA)
4. [ ] **PATCH 2.1** - Dashboard API (MEDIA)
5. [ ] **PATCH 4.1** - Config Validator (MEDIA)
6. [ ] **PATCH 3.1** - OTA Server Doc (BASSA)
7. [ ] **PATCH 4.2** - Config Server (BASSA)

### Test Finale
- [ ] Test completo: pompa via HTTP + MQTT
- [ ] Test deep sleep → wakeup → verifica stato
- [ ] Test OTA update
- [ ] Test config update via MQTT
- [ ] Test dashboard con dati storici

---

## 🔧 Comandi Utili

### Build e Test
```bash
# Build per test
make ENV=esp32-test config
make build

# Build per produzione
make ENV=esp32-prod config
make flash

# Test Wokwi
make test
```

### Debug
```bash
# Monitor seriale
make monitor

# Telnet logger (se configurato)
telnet <ESP32_IP> 23
```

### OTA
```bash
# Upload al server OTA
make ota

# OTA diretto
make ota-direct
```

---

## 📝 Note Finali

- Ogni patch è **atomica** e testabile in isolamento
- Mantenere **compatibilità retroattiva** con config.json esistente
- Documentare **breaking changes** se necessari
- Testare su **hardware reale** prima di deploy produzione

