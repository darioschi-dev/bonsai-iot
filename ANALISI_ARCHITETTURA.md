# 📊 Analisi Architettura - Bonsai IoT ESP32

## 🏗️ Stato attuale (repository firmware)
- **Firmware ESP32** (questo repo): lettura umidità, controllo pompa, MQTT, OTA da manifest, webserver opzionale, Telnet logger, deep sleep, Syslog+MQTT logging.
- **Backend/OTA**: atteso server HTTP con `/firmware/manifest.json` (URL da config). Non incluso qui.
- **Dashboard web**: UI locale in `data/index.html`; eventuale dashboard esterna via MQTT non inclusa.

## 🔍 Struttura del codice
```
src/
├─ main.cpp                 # boot, ciclo principale, deep sleep
├─ mqtt.{h,cpp}             # client MQTT + callback pump/config
├─ webserver.{h,cpp}        # API /api/soil, /api/pump/*
├─ pump_controller.{h,cpp}  # stato pompa centralizzato (PUMP_ON/OFF)
├─ config_api.cpp           # load/save config, API HTTP, apply via MQTT
├─ config_validator.{h,cpp} # default di sicurezza + validazione pin/valori
├─ trigger_firmware_check.* # trigger OTA da MQTT con backoff su NVS
├─ update/*                 # UpdateManager + FirmwareUpdateStrategy (manifest)
├─ logger.{h,cpp}           # Syslog + opzionale publish su MQTT
├─ telnet_logger.*          # ESPTelnet per debugging
├─ mirror_serial.*          # wrapper Serial (mirror)
└─ mail.*                   # SMTP client (credenziali hardcoded)
```

## 🚦 Flusso di boot attuale
1. `loadConfig()` da SPIFFS con `config_validator` (default se mancante/corrotto, ma SSID resta vuoto).
2. `setup_wifi()`: connessione blocca fino a `WL_CONNECTED`; set TZ da `config.timezone` (IANA→POSIX) e sync NTP.
3. Logger Syslog, MQTT (`setupMqtt()`), abilita publish log su MQTT.
4. OTA: `UpdateManager` + `FirmwareUpdateStrategy` su manifest; `triggerFirmwareCheck()` per comando MQTT.
5. Init `PumpController` e ripristino stato da RTC se wakeup; webserver opzionale con API pump/soil.
6. Unica lettura sensore in `setup()` → se sotto soglia attiva pompa per `pump_duration`.
7. `loop()`: OTA, MQTT, Telnet; deep sleep dopo `webserver_timeout` (o 2s minimo) se `debug=false`, salvando stato pompa in RTC.

## ✅ Cose già migliorate rispetto al passato
- **Pompa centralizzata**: `PumpController` usato in main/MQTT/webserver; stato persistito su RTC prima del deep sleep.
- **Timezone configurabile**: supporto IANA → POSIX + default Europe/Rome; `FIRMWARE_BUILD` timezone-aware (`scripts/generate_version.py`).
- **Validazione config**: default sicuri e correzione/salvataggio automatico se config invalido (range pin/valori).
- **OTA più robusto**: manifest JSON, confronto versioni `vX.Y.Z`, download chunk con WDT reset, backoff 24h dopo 3 fallimenti.

## ⚠️ Problemi aperti (priorità)
1) **Boot bloccato se manca il config WiFi**  
`loadConfig()` torna `false` se `config.json` assente/corrotta ma lascia l'oggetto con SSID vuoto; `setup_wifi()` cicla all'infinito (`src/main.cpp`). In campo il device resta brickato senza AP/fallback.

2) **Dati suolo/API mai aggiornati**  
`readSoil()` aggiorna `soilValue/soilPercent`, ma l'API `/api/soil` usa `globalSoil/globalPerc` che non vengono mai sincronizzati (`src/webserver.cpp`, `src/webserver.h`). UI locale e API restituiscono sempre 0; MQTT pubblica solo la lettura fatta in `setup()`.

3) **Logica irrigazione eseguita una sola volta**  
Il sensore è letto solo in `setup()`; `measurement_interval` non è usato e in `loop()` non c'è nuova lettura. Con `debug=true` il dispositivo resta attivo ma non misura né controlla la pompa; con `debug=false` dorme dopo `webserver_timeout` ignorando `measurement_interval`.

4) **SMTP hardcoded e non configurabile**  
`src/mail.cpp` usa credenziali Gmail placeholder e non legge il config. La feature è inutilizzabile e introduce rischi se lasciata così (secrets in chiaro, server fisso).

5) **Telemetry grezza** (bassa): batteria pubblicata come `analogRead()` raw, nessun filtro/avg su umidità, WiFi connection loop senza timeout/backoff.
