import json
import shutil

with open("data/config.example.json") as f:
    example = json.load(f)

with open("data/config.json", "w") as f:
    json.dump(example, f, indent=2)

print("✔ File config.json creato da config.example.json")
