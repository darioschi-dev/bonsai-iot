import os
import sys
import json
import shutil

ENV = os.getenv("ENV", None)

if ENV:
    env_name = ENV.replace("esp32-", "")
    source_path = f"data/config.{env_name}.json"
else:
    source_path = "data/config.example.json"

target_path = "data/config.json"

if not os.path.exists(source_path):
    print(f"❌ File {source_path} non trovato.")
    sys.exit(1)

with open(source_path) as f:
    config = json.load(f)

with open(target_path, "w") as f:
    json.dump(config, f, indent=2)

print(f"✔ File {target_path} creato da {source_path}")
