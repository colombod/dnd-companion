"""Generate release metadata for the Waveshare dial's application image."""
import hashlib
import json
from pathlib import Path
import re
import shutil
from urllib.parse import urlparse

TARGET = "waveshare-esp32-s3-knob-touch-lcd-1.8"
ASSET = TARGET + ".bin"
BASE = "https://github.com/colombod/dnd-companion-firmware/releases"
FEED = BASE + "/latest/download/companion.json"
SLOT_SIZE = 3 * 1024 * 1024


def valid_version(version):
    return bool(re.fullmatch(r"(0|[1-9][0-9]{0,3})(\.(0|[1-9][0-9]{0,3})){2}", version))


def https_url(url):
    parsed = urlparse(url)
    return (parsed.scheme == "https" and bool(parsed.hostname) and not parsed.username
            and not parsed.password and not parsed.fragment and len(url) < 384
            and url.isascii() and all(32 < ord(c) < 127 for c in url))


def configure(version, feed, clean, ca=None):
    if not valid_version(version) or not https_url(feed):
        raise ValueError("Use a numeric major.minor.patch version and an HTTPS feed URL.")
    values = dict(COMPANION_VERSION=version, COMPANION_FEED=feed, COMPANION_TARGET=TARGET,
                  COMPANION_IDENTITY=f"DIAL/{TARGET}/{'release' if clean else 'dev'}/{version}")
    if ca:
        values["COMPANION_TEST_CA"] = Path(ca).read_text()
    return "#pragma once\n" + "".join(f"#define {k} {json.dumps(v)}\n" for k, v in values.items())


def package(binary, version, image_url, note, output, ota_from=()):
    if not https_url(image_url) or not valid_version(version):
        raise ValueError("Invalid release URL or version")
    if not 1 <= len(note.encode("utf-8")) <= 96 or any(ord(c) < 32 for c in note):
        raise ValueError("Use a single release note of up to 96 UTF-8 bytes.")
    current = tuple(map(int, version.split(".")))
    if len(ota_from) > 32 or any(not valid_version(v) or tuple(map(int, v.split("."))) >= current for v in ota_from):
        raise ValueError("OTA starting versions must be older valid firmware versions (maximum 32).")
    image = binary.read_bytes()
    marker = f"DIAL/{TARGET}/release/{version}\0".encode()
    if not 0 < len(image) <= SLOT_SIZE or image[0] != 0xe9 or marker not in image:
        raise ValueError("Image size, Espressif header or embedded release identity is invalid.")
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(binary, output / ASSET)
    manifest = dict(format=2, target=TARGET, version=version, size=len(image),
                    sha256=hashlib.sha256(image).hexdigest(), url=image_url, note=note, otaFrom=sorted(set(ota_from)))
    (output / "companion.json").write_text(json.dumps(manifest, indent=2) + "\n")
