#!/usr/bin/env python3
import subprocess, datetime, re, os
from pathlib import Path

# Prova a importare SCons solo se siamo dentro PlatformIO
try:
    from SCons.Script import Import  # type: ignore
    Import("env")
    in_platformio = True
except ImportError:
    in_platformio = False

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

# --- calcolo versione ---
override = os.environ.get("VERSION_OVERRIDE")
if override:
    version = override
else:
    use_next_patch = os.environ.get("USE_NEXT_VERSION") == "1"
    latest_tag = get_latest_semver_tag()
    version = bump_patch(latest_tag) if use_next_patch else latest_tag

commit = safe_git_command(["git", "rev-parse", "--short", "HEAD"], "unknown")
# Build time with timezone info
try:
    # Try to get local timezone-aware datetime
    now = datetime.datetime.now(datetime.timezone.utc).astimezone()
    tz_name = now.strftime("%Z") or "UTC"
    build_time = now.strftime("%Y-%m-%d %H:%M:%S") + " " + tz_name
except:
    # Fallback to simple format if timezone not available
    build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

header_path = Path("include") / "version_auto.h"
header_path.parent.mkdir(exist_ok=True)

content = f"""#pragma once
#define FIRMWARE_VERSION "{version}"
#define FIRMWARE_COMMIT  "{commit}"
#define FIRMWARE_BUILD   "{build_time}"
"""

if not header_path.exists() or header_path.read_text() != content:
    print(f"[version] Generating {header_path} -> {version} ({commit})")
    header_path.write_text(content)
else:
    print(f"[version] {header_path.name} up to date ({version})")

if in_platformio:
    env.Append(BUILD_FLAGS=[f'-DPROJECT_VER="{version}"'])
