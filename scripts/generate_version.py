#!/usr/bin/env python3

import subprocess
import datetime
from pathlib import Path
import re

import os

use_next_patch = os.environ.get("USE_NEXT_VERSION") == "1"


def safe_git_command(command, fallback):
    try:
        return subprocess.check_output(command).decode().strip()
    except:
        return fallback

def get_latest_semver_tag():
    tags = safe_git_command(["git", "tag"], "")
    version_tags = [tag for tag in tags.splitlines() if re.match(r"^v\d+\.\d+\.\d+$", tag)]
    if not version_tags:
        return "v0.0.0"
    version_tags.sort(key=lambda s: list(map(int, s.lstrip("v").split("."))))
    return version_tags[-1]

def bump_patch(version):
    major, minor, patch = map(int, version.lstrip("v").split("."))
    return f"v{major}.{minor}.{patch + 1}"

# Determina la versione
latest_tag = get_latest_semver_tag()
version = bump_patch(latest_tag) if use_next_patch else latest_tag

# Info commit e build time
commit = safe_git_command(["git", "rev-parse", "--short", "HEAD"], "unknown")
build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

# Genera contenuto
content = f"""#pragma once

#define FIRMWARE_VERSION "{version}"
#define FIRMWARE_COMMIT  "{commit}"
#define FIRMWARE_BUILD   "{build_time}"
"""

# Scrivi su file
header_path = Path("src/version_auto.h")
if not header_path.exists() or header_path.read_text() != content:
    print(f"[INFO] Generating version_auto.h with version {version}, commit {commit}")
    header_path.write_text(content)
else:
    print(f"[INFO] version_auto.h already up to date.")

# Se richiesto, crea e push il nuovo tag
if use_next_patch:
    print(f"[INFO] Tagging and pushing: {version}")
    subprocess.run(["git", "tag", version])
    subprocess.run(["git", "push", "origin", version])
