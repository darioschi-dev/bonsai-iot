#!/usr/bin/env python3
# Extra script per PlatformIO: genera include/version_auto.h e imposta PROJECT_VER
import subprocess
import datetime
import re
from pathlib import Path

# Importa env di PlatformIO
from SCons.Script import Import  # type: ignore
Import("env")

import os

def safe_git_command(cmd, fallback):
    try:
        return subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        return fallback

def get_latest_semver_tag():
    tags = safe_git_command(["git", "tag"], "")
    version_tags = [t for t in tags.splitlines() if re.match(r"^v\d+\.\d+\.\d+$", t)]
    if not version_tags:
        return "v0.0.0"
    version_tags.sort(key=lambda s: list(map(int, s.lstrip("v").split("."))))
    return version_tags[-1]

def bump_patch(version):
    major, minor, patch = map(int, version.lstrip("v").split("."))
    return f"v{major}.{minor}.{patch + 1}"

use_next_patch = os.environ.get("USE_NEXT_VERSION") == "1"

latest_tag = get_latest_semver_tag()
version = bump_patch(latest_tag) if use_next_patch else latest_tag

commit = safe_git_command(["git", "rev-parse", "--short", "HEAD"], "unknown")
build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

# Genera header in include/
include_dir = Path("include")
include_dir.mkdir(exist_ok=True)
header_path = include_dir / "version_auto.h"

content = f"""#pragma once
#define FIRMWARE_VERSION "{version}"
#define FIRMWARE_COMMIT  "{commit}"
#define FIRMWARE_BUILD   "{build_time}"
"""

# Scrivi solo se cambia
if not header_path.exists() or header_path.read_text() != content:
    print(f"[version] Generating {header_path} -> {version} ({commit})")
    header_path.write_text(content)
else:
    print(f"[version] {header_path.name} up to date ({version})")

# Esponi anche PROJECT_VER per esp_app_desc_t
env.Append(BUILD_FLAGS=[f'-DPROJECT_VER="{version}"'])

# Se richiesto, crea e push il nuovo tag (idempotente)
if use_next_patch:
    # Evita errore se il tag esiste già
    existing = safe_git_command(["git", "tag", "--list", version], "")
    if version not in existing.splitlines():
        subprocess.run(["git", "tag", version], check=False)
        subprocess.run(["git", "push", "origin", version], check=False)
        print(f"[version] Tagged {version}")
    else:
        print(f"[version] Tag {version} already exists, skip push")
