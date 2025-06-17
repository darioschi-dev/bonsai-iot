import os
import sys
import json

REQUIRED_KEYS = [
    "wifi_ssid",
    "wifi_password",
    "mqtt_broker",
    "mqtt_username",
    "mqtt_password",
    "mqtt_port"
]

ENV = os.getenv("ENV", None)
env_name = ENV.replace("esp32-", "") if ENV else "example"
source_path = f"data/config.{env_name}.json"
target_path = "data/config.json"

if not os.path.exists(source_path):
    print(f"❌ File {source_path} non trovato.")
    sys.exit(1)

with open(source_path) as f:
    config = json.load(f)

# Validazione delle sole chiavi richieste
missing_keys = [k for k in REQUIRED_KEYS if k not in config or config[k] in ("", None)]
if missing_keys:
    print(f"❌ Mancano le seguenti chiavi obbligatorie in {source_path}:")
    for key in missing_keys:
        print(f" - {key}")
    sys.exit(1)

# Scrittura config.json
with open(target_path, "w") as f:
    json.dump(config, f, indent=2)

print(f"✔ File {target_path} creato da {source_path}")
