# 🌱 ESP32 Igrometro – Monitoraggio Umidità del Suolo

![PlatformIO](https://img.shields.io/badge/platformio-ready-orange?logo=platformio)

Sistema basato su **ESP32** per il monitoraggio dell’umidità del suolo e il controllo automatico di una pompa di irrigazione. Include:

* Interfaccia web locale (Web UI)
* Integrazione opzionale con MQTT
* Notifiche via email (SMTP)
* Deep Sleep per risparmio energetico
* Simulazione con **Wokwi CLI** per test online

---

## 🚀 Setup rapido

### 1. Clona il progetto

```bash
git clone https://github.com/darioschi-dev/bonsai-iot.git
cd bonsai-iot
```

### 2. Genera il file `config.json`

```bash
python scripts/setup_config.py
```

> ⚠️ Il file `config.json` viene generato da `config.example.json` e **non è incluso nel repository** per motivi di sicurezza.

### 3. Carica firmware e SPIFFS (ambiente reale)

```bash
make flash
```

### 4. Esegui simulazione in Wokwi (ambiente test)

1. Inserisci il tuo token nel file `.env`:

```
WOKWI_CLI_TOKEN=wok_XXXXXXXXXXXXXXXXXXXXXXXXX
```

2. Avvia la simulazione:

```bash
make test
```

---

## 📁 Struttura del progetto

```
.
├── data/                   # Contenuti SPIFFS
│   ├── config.example.json
│   ├── config.prod.json
│   ├── config.json
│   └── index.html
├── diagram.json            # Schema Wokwi (simulazione)
├── include/                # Header files
├── lib/                    # Librerie custom
├── platformio.ini          # Configurazione PIO
├── Readme.md               # Questo file
├── scripts/
│   ├── setup_config.py     # Genera config.json
│   └── uploadfs.py         # Upload SPIFFS post-firmware
├── src/                    # Codice sorgente ESP32
├── test/                   # Test hardware/futuri
├── wokwi.toml              # Configurazione simulazione Wokwi
└── .env                    # Token Wokwi (non tracciato)
```

---

## ⚙️ Configurazione

Modifica `data/config.example.json` per configurare:

* SSID/Password Wi-Fi
* Email mittente/destinatario
* Soglia umidità e tempi di attivazione pompa
* Pin GPIO utilizzati

---

## 🧪 Testing

La simulazione online è basata su [Wokwi CLI](https://docs.wokwi.com/cli/overview):

### 🧰 Installazione Wokwi CLI

1. Scarica da: [https://github.com/wokwi/wokwi-cli/releases](https://github.com/wokwi/wokwi-cli/releases)
2. Rendi eseguibile:

   ```bash
   chmod +x wokwi-cli
   mv wokwi-cli /usr/local/bin/
   ```
3. Verifica:

   ```bash
   wokwi-cli --version
   ```

### 🆕 Inizializza Wokwi (solo la prima volta)

```bash
wokwi-cli init
```

> ⚠️ Sovrascrive `wokwi.toml` e `diagram.json`. Fai un backup se necessario.

### ♻️ Per ripristinare i tuoi file custom

```bash
cp wokwi-presets/diagram.json diagram.json
cp wokwi-presets/wokwi.toml wokwi.toml
```

---

## 📡 MQTT (opzionale)

Il codice supporta l’uso di broker MQTT per l’invio e ricezione di dati. Broker consigliati:

- [HiveMQ Cloud](https://console.hivemq.cloud/)
- [Mosquitto](https://mosquitto.org/)

---

## 🔐 Sicurezza

* `config.json` e `.env` sono ignorati da Git
* Non committare dati sensibili

---

## ✅ TODO

* [ ] UI HTML migliorata
* [ ] Dashboard MQTT (es. Node-RED)
* [ ] Logging persistente
* [ ] OTA update via `/update`

---

© MIT License – Dario Schiavano