import subprocess
import datetime
from pathlib import Path

# Funzione per eseguire comandi shell con fallback
def safe_git_command(command, fallback):
    try:
        return subprocess.check_output(command).decode().strip()
    except:
        return fallback

# Ottieni l'ultima versione da tag, oppure fallback a v0.0.0
version = safe_git_command(["git", "describe", "--tags", "--always"], "v0.0.0")

# Ottieni l'hash short dell'ultimo commit
commit = safe_git_command(["git", "rev-parse", "--short", "HEAD"], "unknown")

# Timestamp build (locale)
build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

# Contenuto del file da generare
content = f"""#pragma once

#define FIRMWARE_VERSION "{version}"
#define FIRMWARE_COMMIT  "{commit}"
#define FIRMWARE_BUILD   "{build_time}"
"""

# Percorso completo del file da scrivere
header_path = Path("src/version_auto.h")

# Scrivi solo se è cambiato
if not header_path.exists() or header_path.read_text() != content:
    print(f"[INFO] Generating version_auto.h with version {version}, commit {commit}")
    header_path.write_text(content)
else:
    print(f"[INFO] version_auto.h already up to date.")
