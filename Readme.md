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

> Carica firmware e file SPIFFS sull’ESP32 reale (`ENV=esp32-prod` di default).

### 4. Esegui simulazione in Wokwi (ambiente test)

1. Inserisci il tuo token nel file `.env`:

```env
WOKWI_CLI_TOKEN=wok_XXXXXXXXXXXXXXXXXXXXXXXXX
```

2. Avvia la simulazione:

```bash
make test
```

> Compila, carica SPIFFS virtuale e avvia la simulazione online con il file system montato.

---

## 📁 Struttura del progetto

```
.
├── data/                   # Contenuti SPIFFS
│   ├── config.example.json
│   ├── config.prod.json
│   ├── config.test.json
│   ├── config.json         # Generato dinamicamente
│   └── index.html
├── diagram.json            # Schema hardware Wokwi
├── include/                # Header files (config.h, mail.h, ecc.)
├── lib/                    # Librerie custom (se presenti)
├── platformio.ini          # Configurazione build PIO
├── scripts/
│   ├── setup_config.py     # Script per generare config.json
│   └── uploadfs.py         # Upload automatico SPIFFS post-upload
├── src/                    # Codice principale
│   └── main.cpp
├── test/                   # Test futuri
├── wokwi.toml              # Configurazione simulazione Wokwi
├── .env                    # Token Wokwi (non tracciato)
├── .gitignore              # Protezione file sensibili
├── Makefile                # Comandi build e test
└── README.md               # Questo file
```

---

## 🛠️ Comandi `make`

| Comando        | Descrizione |
|----------------|-------------|
| `make`         | Compila, carica firmware e SPIFFS, apre il monitor seriale |
| `make flash`   | Come sopra, ma forzato su ambiente reale |
| `make config`  | Copia `data/config.{ENV}.json` → `data/config.json` |
| `make test`    | Compila in `esp32-test`, monta SPIFFS, avvia simulazione Wokwi |
| `make upload`  | Carica solo il firmware (no SPIFFS) |
| `make uploadfs`| Carica solo SPIFFS (se `ENV != esp32-test`) |
| `make monitor` | Apre il monitor seriale |
| `make clean`   | Pulisce la build per l’ambiente attivo |
| `make setup-config` | Rigenera `config.json` da template |

---

## ⚙️ Configurazione

Modifica `data/config.example.json` (e versioni `.prod` / `.test`) per:

* SSID/Password Wi-Fi
* Email mittente e destinatario
* Broker MQTT (opzionale)
* Soglia umidità (0–100 %)
* Pin GPIO per LED, sensore, pompa
* Debug e durata deep sleep

---

## 🧪 Simulazione con Wokwi CLI

### 🧰 Installazione Wokwi CLI

1. Scarica da: [https://github.com/wokwi/wokwi-cli/releases](https://github.com/wokwi/wokwi-cli/releases)
2. Rendi eseguibile:

   ```bash
   chmod +x wokwi-cli
   sudo mv wokwi-cli /usr/local/bin/
   ```

3. Verifica:

   ```bash
   wokwi-cli --version
   ```

### ⚙️ Inizializza Wokwi (solo 1 volta)

```bash
wokwi-cli init
```

> ⚠️ Sovrascrive `wokwi.toml` e `diagram.json`

### ♻️ Ripristina preset personalizzati

```bash
cp wokwi-presets/diagram.json diagram.json
cp wokwi-presets/wokwi.toml wokwi.toml
```

---

## 📡 MQTT (opzionale)

Broker supportati:

- [HiveMQ Cloud](https://console.hivemq.cloud/)
- Mosquitto (locale o remoto)

---

## 🔐 Sicurezza

* `.env` e `config.json` sono esclusi da Git
* Nessuna credenziale viene salvata nel repo

---

## ✅ TODO

* [ ] UI HTML migliorata
* [ ] Dashboard MQTT (es. Node-RED)
* [ ] Logging persistente
* [ ] OTA update via `/update`

---

© MIT License – Dario Schiavano
