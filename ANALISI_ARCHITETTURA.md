# 📊 Analisi Architettura - Bonsai IoT ESP32

## 🏗️ Architettura Attuale

### Componenti Principali

1. **Firmware ESP32** (questo repository)
   - Monitoraggio umidità suolo
   - Controllo pompa di irrigazione
   - Web server locale (AsyncWebServer)
   - Client MQTT (PubSubClient)
   - OTA updates (HTTP + manifest.json)
   - Deep sleep per risparmio energetico
   - Logging (Syslog + MQTT)

2. **Server Node.js** (repository separato: `bonsai-mqtt-dashboard`)
   - REST API per OTA firmware upload
   - Broker MQTT (o integrazione con broker esterno)
   - Storage dati (MongoDB menzionato nel README)
   - Endpoint `/firmware/manifest.json` per OTA

3. **Dashboard Vue** (repository separato: `bonsai-mqtt-dashboard`)
   - Visualizzazione dati real-time
   - Controllo remoto pompa
   - Grafici storici (da implementare)

---

## 🔍 Analisi Dettagliata del Firmware ESP32

### Struttura Moduli

```
src/
├── main.cpp              # Entry point, setup/loop
├── config.h/.cpp         # Struttura Config e load/save
├── config_api.cpp        # API HTTP + MQTT per config
├── mqtt.cpp/.h           # Client MQTT, callback, publish
├── webserver.cpp/.h      # Server HTTP locale (minimal)
├── logger.cpp/.h         # Logging Syslog + MQTT
├── mail.cpp/.h           # Notifiche email SMTP
├── update/
│   ├── UpdateManager.h   # Gestore strategie update
│   ├── FirmwareUpdateStrategy.cpp
│   └── UpdateStrategy.h  # Interfaccia strategia
└── triggerFirmwareCheck.cpp  # Trigger OTA manuale
```

### Flusso Operativo

1. **Setup()**
   - Carica config da SPIFFS
   - Connessione WiFi
   - Sync NTP (timezone hardcoded: CET-1CEST)
   - Setup MQTT
   - Check OTA firmware
   - Setup webserver
   - Lettura sensore
   - Se umidità < soglia → accendi pompa
   - Deep sleep (se `debug=false`)

2. **Loop()** (solo se `debug=true`)
   - ArduinoOTA.handle()
   - loopMqtt()
   - loopTelnetLogger()
   - Watchdog reset

---

## ⚠️ Problemi e Bug Identificati

### 1. **BUG POMPA ON/OFF** 🔴 CRITICO

**Problema:**
- In `webserver.cpp`: `/api/pump/on` e `/api/pump/off` modificano direttamente il pin senza sincronizzazione
- In `mqtt.cpp`: callback MQTT modifica il pin senza notificare webserver
- Stato pompa letto da `digitalRead()` ma non c'è stato centralizzato
- Possibile race condition tra HTTP e MQTT

**File coinvolti:**
- `src/webserver.cpp` (linee 28-35)
- `src/mqtt.cpp` (linee 142-168)
- `src/main.cpp` (linee 137-155)

**Rischio:** Stato inconsistente, pompa può rimanere accesa/spenta

---

### 2. **BUG DEEP SLEEP** 🔴 CRITICO

**Problema:**
- Deep sleep attivato solo se `config.debug == false` (linea 238 main.cpp)
- Dopo deep sleep, il device si riavvia ma:
  - Variabili globali non-RTC vengono perse
  - `bootCount` è `RTC_DATA_ATTR` ✅ OK
  - Ma `soilValue`, `soilPercent`, `globalSoil`, `globalPerc` vengono persi
  - Il webserver non funziona dopo wakeup (serve ri-inizializzazione)
  - MQTT si riconnette ma lo stato pompa potrebbe essere inconsistente

**File coinvolti:**
- `src/main.cpp` (linee 238-243)
- `src/webserver.cpp` (variabili globali)

**Rischio:** Dati persi, stato inconsistente dopo wakeup

---

### 3. **BUG TIMESTAMP/TIMEZONE** 🟡 MEDIO

**Problema:**
- Timezone hardcoded: `"CET-1CEST,M3.5.0/2,M10.5.0/3"` (linea 118 main.cpp)
- `FIRMWARE_BUILD` generato a build-time senza timezone (linea 41 generate_version.py)
- `epochMs()` restituisce millisecondi UTC, ma non c'è conversione timezone
- Timestamp pubblicati su MQTT sono UTC, dashboard potrebbe mostrare orari sbagliati

**File coinvolti:**
- `src/main.cpp` (linea 118)
- `scripts/generate_version.py` (linea 41)
- `src/mqtt.h` (linee 39-45)

**Rischio:** Timestamp errati nella dashboard, build info senza timezone

---

### 4. **PROBLEMI ARCHITETTURALI** 🟡

#### A. Gestione Config
- Config caricato da SPIFFS ma non validato
- Nessun fallback se config.json corrotto
- `config_version` confrontato come stringa (linea 27 mqtt.cpp) → vulnerabile a formati diversi

#### B. OTA Server
- OTA server esterno (Node.js) non integrato nel backend
- Makefile fa riferimento a `../bonsai-mqtt-dashboard` ma non è in questo repo
- Endpoint OTA hardcoded nel config

#### C. Dashboard
- Dashboard Vue menzionata ma non presente
- Web UI locale (`index.html`) è solo redirect alla dashboard esterna
- Nessuna sincronizzazione stato pompa tra HTTP e MQTT

#### D. Organizzazione Codice
- Variabili globali sparse (`globalSoil`, `globalPerc`, `soilValue`, `soilPercent`)
- Nessuna classe per gestione pompa centralizzata
- Logica pompa duplicata (main.cpp, mqtt.cpp, webserver.cpp)

---

## ✅ Miglioramenti Strutturali Proposti

### 1. **Centralizzazione Gestione Pompa**
Creare classe `PumpController`:
- Stato centralizzato (on/off)
- Mutex per thread-safety
- Notifiche MQTT + HTTP sincronizzate
- Persistenza stato su Preferences

### 2. **Gestione Deep Sleep Migliorata**
- Salvare stato critico in RTC_DATA_ATTR
- Ri-inizializzare webserver dopo wakeup
- Validare stato pompa dopo wakeup

### 3. **Config Management Robusto**
- Validazione schema JSON
- Fallback a default se corrotto
- Versioning semantico per `config_version`

### 4. **Timezone Configurabile**
- Aggiungere `timezone` in Config
- Usare timezone per `FIRMWARE_BUILD`
- Pubblicare timestamp con timezone info

### 5. **Refactoring State Management**
- Classe `DeviceState` per variabili globali
- Singleton pattern per stato condiviso
- Eliminare duplicazioni

---

## 📋 Piano Operativo - Patch Atomiche

### **FASE 1: Fix Bug Critici**

#### **PATCH 1.1: Fix Pompa ON/OFF**
**File:** `src/pump_controller.h` (NUOVO), `src/pump_controller.cpp` (NUOVO)
- Creare classe `PumpController` con stato centralizzato
- Mutex per accesso thread-safe
- Metodi `turnOn()`, `turnOff()`, `getState()`

**File da modificare:**
- `src/main.cpp` → usa `PumpController`
- `src/mqtt.cpp` → usa `PumpController`
- `src/webserver.cpp` → usa `PumpController`

**Ordine applicazione:**
1. Creare `pump_controller.h` e `pump_controller.cpp`
2. Modificare `main.cpp` (setup, turnOnPump, turnOffPump)
3. Modificare `mqtt.cpp` (callback pump)
4. Modificare `webserver.cpp` (endpoint API)

---

#### **PATCH 1.2: Fix Deep Sleep**
**File:** `src/main.cpp`, `src/webserver.cpp`
- Salvare stato pompa in RTC_DATA_ATTR prima di sleep
- Ri-inizializzare webserver dopo wakeup
- Validare stato dopo wakeup

**Ordine applicazione:**
1. Aggiungere `RTC_DATA_ATTR` per stato pompa
2. Modificare `setup()` per ri-inizializzazione post-wakeup
3. Test wakeup → verifica stato

---

#### **PATCH 1.3: Fix Timestamp/Timezone**
**File:** `src/config.h`, `src/main.cpp`, `scripts/generate_version.py`
- Aggiungere campo `timezone` in Config
- Usare timezone per `setenv("TZ", ...)`
- Modificare `generate_version.py` per includere timezone nel build time

**Ordine applicazione:**
1. Aggiungere `timezone` in `config.h` e `config.example.json`
2. Modificare `main.cpp` setup_wifi() per usare config.timezone
3. Modificare `generate_version.py` per timezone-aware build time
4. Aggiornare `config_api.cpp` per serializzare/deserializzare timezone

---

### **FASE 2: Miglioramenti Dashboard**

#### **PATCH 2.1: Miglioramento Dashboard (Usabilità + Grafici)**
**Nota:** Dashboard Vue è in repository separato. Questa patch riguarda:
- API REST migliorate per dati storici
- Endpoint `/api/history` per grafici
- Endpoint `/api/stats` per statistiche

**File da creare/modificare:**
- `src/webserver.cpp` → aggiungere endpoint `/api/history`, `/api/stats`
- `src/data_logger.h/.cpp` (NUOVO) → logging dati su SPIFFS o Preferences

**Ordine applicazione:**
1. Creare `data_logger.h/.cpp` per storage locale
2. Modificare `webserver.cpp` per endpoint history/stats
3. Integrare logging in `main.cpp` loop

---

### **FASE 3: Setup OTA Server Integrato**

#### **PATCH 3.1: Setup OTA Server Integrato nel Node Backend**
**Nota:** Richiede accesso al repository Node.js. Questa patch documenta:
- Endpoint `/firmware/upload` già presente (da verificare)
- Endpoint `/firmware/manifest.json` per OTA
- Validazione SHA256
- Storage firmware su filesystem

**File da verificare/modificare (nel repo Node.js):**
- `server.js` o `app.js` → endpoint OTA
- `routes/firmware.js` → gestione upload
- `public/firmware/` → directory storage

**Ordine applicazione:**
1. Verificare struttura repo Node.js
2. Documentare endpoint esistenti
3. Aggiungere validazione SHA256 se mancante
4. Test upload firmware

---

### **FASE 4: Gestione Config Migliorata**

#### **PATCH 4.1: Miglior Gestione config.json lato ESP32**
**File:** `src/config.h`, `src/config_api.cpp`
- Validazione schema JSON
- Fallback a default se corrotto
- Versioning semantico

**Ordine applicazione:**
1. Creare `config_validator.h/.cpp` (NUOVO)
2. Modificare `loadConfig()` per validazione
3. Aggiungere default config in caso di errore

---

#### **PATCH 4.2: Miglior Gestione config.json lato Server**
**Nota:** Richiede accesso al repository Node.js
- API REST per gestione config remota
- Validazione lato server
- Versioning config

**File da verificare/modificare (nel repo Node.js):**
- `routes/config.js` → API config
- Validazione schema JSON

---

## 🎯 Priorità Esecuzione

1. **URGENTE:** PATCH 1.1 (Fix Pompa) - Bug critico produzione
2. **URGENTE:** PATCH 1.2 (Fix Deep Sleep) - Bug critico produzione
3. **ALTA:** PATCH 1.3 (Fix Timestamp) - Problema usabilità
4. **MEDIA:** PATCH 2.1 (Dashboard) - Miglioramento UX
5. **MEDIA:** PATCH 4.1 (Config ESP32) - Robustezza
6. **BASSA:** PATCH 3.1 (OTA Server) - Se già funzionante, solo documentazione
7. **BASSA:** PATCH 4.2 (Config Server) - Se già funzionante, solo miglioramenti

---

## 📝 Note Implementazione

### Convenzioni Codice
- Usare `const` dove possibile
- Evitare `String` in interrupt/hot path (usare `char*`)
- Preferire `StaticJsonDocument` a `DynamicJsonDocument` quando possibile
- Documentare funzioni pubbliche con commenti

### Testing
- Testare ogni patch in isolamento
- Verificare comportamento con `debug=true` e `debug=false`
- Test deep sleep wakeup
- Test pompa via HTTP e MQTT simultaneamente

### Compatibilità
- Mantenere compatibilità con config.json esistente
- Aggiungere campi opzionali, non rimuovere esistenti
- Versioning per breaking changes

