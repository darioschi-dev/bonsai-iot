# CLAUDE.md — Bonsai Firmware

## Scopo

Firmware ESP32 per monitoraggio bonsai: legge umidità del suolo (ADC), temperatura, livello batteria, controlla pompa d'irrigazione e invia telemetria via MQTT. Si aggiorna OTA tramite il backend del repo gemello `personal/bonsai-dashboard`. Supporta deep sleep dinamico per risparmio energetico e simulazione via Wokwi CLI per sviluppo senza hardware.

## Stack

PlatformIO + Arduino framework, ESP32 (`esp32doit-devkit-v1`), C++. Librerie principali: `PubSubClient` (MQTT), `ArduinoJson`, `ArduinoOTA`, `WiFi`, `Wire`, `esp_ota_ops.h`. Build default: `esp32-prod`. Ambiente test: `esp32-test` (Wokwi).

## Architettura — ciclo di boot

```
setup():
  loadConfig()       ← SPIFFS: data/config.json (validato da config_validator.cpp)
  setup_wifi()       ← WiFi.begin() — blocking (bug A: nessun timeout)
  NTP sync           ← timeout 10s; se fallisce, timestamp 1970 (bug G)
  setupMqtt()        ← PubSubClient → config.mqtt_broker:mqtt_port
  UpdateManager      ← FirmwareUpdateStrategy: fetch manifest → compare → OTA
  readSoil()         ← median 5 letture ADC sensor_pin (chiamata UNA SOLA VOLTA — bug B)
  pump check         ← se soilPercent < moisture_threshold → pumpController.activate()
  ArduinoOTA.begin()

loop():
  WiFi / MQTT / OTA handle
  webserver (se enable_webserver)
  Telnet logger
  publish telemetry  ← ogni mqttInterval (15s fisso)
  → deep sleep       ← se !debug, dopo webserver_timeout
```

Il sensore viene letto una sola volta in `setup()`. Il campo `measurement_interval` in config è letto ma non consumato dal loop (bug critico Category B).

## File sorgente chiave

```
src/
├── main.cpp                     ← boot sequence, loop, deep sleep, OTA check
├── mqtt.cpp / mqtt.h            ← publish telemetria (intervallo 15s), subscribe comandi
├── pump_controller.cpp/.h       ← ON/OFF pompa, persistenza stato in RTC_DATA_ATTR
├── config_api.cpp/.h            ← load/save config da SPIFFS
├── config_validator.cpp/.h      ← validazione e defaults
├── update/
│   ├── UpdateManager.h          ← registra strategie OTA
│   └── FirmwareUpdateStrategy.cpp ← fetch manifest HTTP, compare versione, flash
├── webserver.cpp/.h             ← Web UI locale (PIN auth, dark mode)
├── logger.cpp / telnet_logger.cpp ← Syslog UDP + Telnet debug
data/
├── config.example.json          ← template con tutte le chiavi
├── config.prod.json             ← valori produzione (non committare)
├── config.test.json             ← valori Wokwi
└── config.json                  ← generato dinamicamente (gitignored)
```

## Configurazione SPIFFS

`data/config.json` generato da `scripts/setup_config.py` o dal Makefile. Non va committato.

```bash
python scripts/setup_config.py   # interattivo, genera da config.example.json
make config ENV=esp32-prod       # copia config.prod.json → config.json
make config ENV=esp32-test       # copia config.test.json → config.json
```

Chiavi operative principali:
```json
{
  "wifi_ssid": "", "wifi_password": "",
  "mqtt_broker": "", "mqtt_port": 1883,
  "mqtt_username": "", "mqtt_password": "",
  "sensor_pin": 34, "pump_pin": 26, "battery_pin": 35,
  "moisture_threshold": 40,
  "pump_duration": 10000,
  "measurement_interval": 300,
  "sleep_hours": 0,
  "ota_manifest_url": "http://bonsai-iot-update.darioschiavano.it/api/ota/manifest",
  "config_version": 1,
  "debug": false
}
```

Con `sleep_hours=0` la durata del deep sleep è dinamica: umidità >70% → 1h, >50% → 30min, >30% → 15min, ≤30% → 5min.

## OTA — aggiornamento firmware

Due meccanismi:

**ArduinoOTA (UDP locale):** attivo in `loop()`, usato solo in sviluppo locale (`make ota-direct`).

**FirmwareUpdateStrategy (HTTP manifest-based, principale):** all'avvio l'ESP32 fetcha `ota_manifest_url`, confronta la versione corrente (macro `FIRMWARE_VERSION` generata da `scripts/generate_version.py`), scarica il `.bin` dal backend dashboard, flasha via `esp_ota_ops.h`. Rollback automatico se il primo boot post-OTA fallisce.

Dopo un boot post-OTA riuscito va confermato esplicitamente:
```cpp
esp_ota_mark_app_valid_cancel_rollback();  // in setup() dopo validazione
```

Il `FIRMWARE_VERSION` è generato automaticamente da tag Git: `make release` esegue bump patch + tag.

## MQTT — contratto con bonsai-dashboard

Il firmware pubblica con field name esatti — non rinominare senza aggiornare anche `src/server.ts` nel dashboard:
```
bonsai/<device_id>/status/humidity    temp    battery    wifi    firmware
bonsai/<device_id>/health/watchdog    ← heartbeat ogni 30s
```

Il firmware si iscrive a:
```
bonsai/<device_id>/command/pump       ← {"pump":"on"|"off"|"toggle"}
bonsai/<device_id>/config             ← JSON + config_version (applica se più recente)
bonsai/<device_id>/config/set         ← applica senza versioning
```

MQTT backoff esponenziale: riconnessione con delay 2s→4s→8s→…→60s max. Intervallo pubblicazione fisso a 15s — nessun throttle in caso di burst (bug C).

## Comandi utili

```bash
make flash          # Compila, carica firmware + SPIFFS su ESP32 reale, apre monitor
make upload         # Solo firmware (no SPIFFS)
make uploadfs       # Solo SPIFFS (aggiorna config senza reflash firmware)
make monitor        # Monitor seriale
make config         # Copia config.{ENV}.json → config.json (default ENV=esp32-prod)
make test           # Compila esp32-test, monta SPIFFS virtuale, avvia simulazione Wokwi
make release        # Build + bump patch + tag Git automatico
make ota            # Upload OTA al backend remoto (bonsai-iot-update.darioschiavano.it)
make ota-local      # Upload OTA al backend locale (127.0.0.1:8081)
make ota-direct     # OTA diretto all'ESP32 via espota (dev locale)
make clean          # Pulisce build per l'ambiente attivo
```

## Simulazione Wokwi

Per sviluppare senza ESP32 reale:
```bash
# Prerequisito: wokwi-cli installato + token in .env
WOKWI_CLI_TOKEN=wok_xxx
make test
```

Se `wokwi-cli init` sovrascrive i preset:
```bash
cp wokwi-presets/diagram.json diagram.json
cp wokwi-presets/wokwi.toml wokwi.toml
```

## Branch non mergiato: `fix/firmware-critical-issues`

Il repo è su questo branch da circa 5 mesi. Contiene fix a bug critici non ancora mergiati su `master`. Verificare il diff prima di sviluppare nuove feature per non creare conflitti. Decidere se chiudere il merge o abbandonare il branch prima di procedere con sviluppo significativo.

## Bug aperti — triage 20-01-2026

**Category A — WiFi boot hang (critico):** `WiFi.begin()` senza timeout né fallback. Se l'AP non risponde, il device si blocca all'infinito prima di connettersi a MQTT. Il README descrive un AP fallback "Bonsai-Setup-{deviceId}" ma il triage del codice mostrava ancora il loop infinito — verificare se implementato nel branch `fix/firmware-critical-issues`.

**Category B — Sensore mai riletto (critico):** `readSoil()` chiamato una sola volta in `setup()`. `measurement_interval` è letto ma mai usato nel `loop()`. Con `debug=false` ogni wake da deep sleep è un nuovo boot, quindi ogni ciclo parte da un nuovo `readSoil()`. Con `debug=true` il device resta sveglio con il valore stale dell'avvio.

**Category H — RTC corruption al power reset (importante):** `RTC_DATA_ATTR` per `bootCount` e `pumpStateAfterWakeup` può corrompersi se il power viene rimosso durante deep sleep. Sintomo: pompa stuck ON o mai attiva dopo power cut. Fix: checksum sui dati RTC.

**Category I — OTA manifest fetch no timeout (importante):** se `ota_manifest_url` non raggiungibile, il device si blocca durante il boot check OTA. La FirmwareUpdateStrategy dovrebbe avere timeout 10s manifest / 30s download — verificare in `src/update/FirmwareUpdateStrategy.cpp`.

**Category J — JSON payload overflow (medio):** `StaticJsonDocument<1024>` potrebbe non bastare per config JSON estese. Parse failure silente: comandi MQTT ignorati senza log.

**Pump failsafe (HIGH priority — non implementato):** la pompa non ha limite massimo di attivazione. Se non riceve comando di stop, può restare ON indefinitamente. Fix: `maxRunMs = 60000ms` con emergency stop e pubblicazione su `bonsai/<device_id>/alert/pump`.

---

Workflow, ruoli e testing: `~/Projects/docs/ai-governance/v3/`
