"""Build the dial with pinned, project-local Waveshare drivers and libraries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
import firmware_release as release
from spin_art import lvgl_frames
from locale_assets import prepare_languages, with_accents

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / ".build"
LIBS = BUILD / "libraries"
SKETCH = BUILD / "Companion"
VENDOR = "ESP32-S3-Knob-Touch-LCD-1.8-Demo/Arduino/"
PACKAGES = {
    "waveshare": ("https://files.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8/ESP32-S3-Knob-Touch-LCD-1.8-Demo.zip",
                  "11e382444fe93470fbe463829c1e0ebad5bdb5115fd2d72f6159cd7700015030"),
    "SensorLib": ("https://downloads.arduino.cc/libraries/github.com/lewisxhe/SensorLib-0.3.1.zip",
                  "92b3f716f8a6048eaec7876b19089a7dfdb009c9fac7cc6c24fee4177f32587a"),
    "ArduinoJson": ("https://downloads.arduino.cc/libraries/github.com/bblanchon/ArduinoJson-7.4.2.zip",
                    "5848dbc2a2eca7eb469710f64106195e90e6788a2365859f88c491214a108230"),
}
FQBN = "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc,USBMode=hwcdc,PartitionScheme=app3M_fat9M_16MB"

def write(path, content):
    if isinstance(content, str):
        content = content.encode()
    if not path.exists() or path.read_bytes() != content:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

def dice_tiles():
    from PIL import Image
    atlas = Image.open(ROOT / "docs/screens/dice-tokens.png").convert("RGBA")
    tiles = []
    for i in range(7):
        tile = atlas.crop((52*i, 0, 52*(i+1), 52))
        colors = tile.convert("RGB").quantize(colors=15)
        rgb = colors.getpalette()
        palette = [0, 0, 0, 0]
        for r, g, b in zip(rgb[0:45:3], rgb[1:45:3], rgb[2:45:3]):
            palette.extend((b, g, r, 255))
        values = [v+1 if a >= 128 else 0 for v,a in zip(colors.tobytes(),tile.getchannel("A").tobytes())]
        packed = palette + [hi*16+lo for hi,lo in zip(values[::2],values[1::2])]
        tiles.append("{"+",".join(map(str,packed))+"}")
    return ",\n".join(tiles)

def prepare(config, clean):
    for name, (url, sha) in PACKAGES.items():
        archive = BUILD / "downloads" / (name + ".zip")
        archive.parent.mkdir(parents=True, exist_ok=True)
        if not archive.exists():
            temp = archive.with_suffix(".partial")
            subprocess.run(["curl", "-fLsS", "--retry", "2", "-o", temp, url], check=True)
            temp.replace(archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != sha:
            sys.exit(f"Checksum mismatch: {archive}")
        with zipfile.ZipFile(archive) as z:
            prefix = VENDOR + "libraries/lvgl/" if name == "waveshare" else z.namelist()[0].split("/")[0] + "/"
            dest = LIBS / ("lvgl" if name == "waveshare" else name)
            font_root = prefix + "scripts/built_in_font/"
            typeface = z.read(font_root + "Montserrat-Medium.ttf") if name == "waveshare" else None
            for member in z.namelist():
                if not member.startswith(prefix) or member.endswith("/"):
                    continue
                rel = member[len(prefix):]
                if rel.startswith("src/") or rel in ("library.properties", "lvgl.h", "LICENSE", "LICENSE.txt"):
                    data = z.read(member)
                    for size in (14, 18, 20, 28, 36):
                        if typeface and rel == f"src/font/lv_font_montserrat_{size}.c":
                            data = with_accents(data.decode(), typeface, size)
                    write(dest / rel, data)
            if name == "waveshare":
                lcd = VENDOR + "examples/08_LVGL_Test/"
                for f in ("esp_lcd_sh8601.c", "esp_lcd_sh8601.h"):
                    write(SKETCH / f, z.read(lcd + f))
                for f in ("bidi_switch_knob.c", "bidi_switch_knob.h"):
                    write(SKETCH / f, z.read(VENDOR + "examples/04_Encoder_Test/" + f))
                bsp = z.read(lcd + "lcd_bsp.c").decode().replace("\r\n", "\n")
                table = bsp[bsp.index("static const sh8601_lcd_init_cmd_t"):bsp.index("void lcd_lvgl_Init")]
                write(SKETCH / "panel_table.inc", table)
                conf = z.read(VENDOR + "libraries/lv_conf.h").decode().replace("\r\n", "\n")
                for size in (18, 20, 28, 36):
                    conf = conf.replace(f"LV_FONT_MONTSERRAT_{size} 0", f"LV_FONT_MONTSERRAT_{size} 1")
                conf = conf.replace("LV_USE_DEMO_WIDGETS 1", "LV_USE_DEMO_WIDGETS 0")
                conf = conf.replace("LV_USE_QRCODE 0", "LV_USE_QRCODE 1")
                write(LIBS / "lv_conf.h", conf)
    for path in (ROOT / "firmware/Companion").iterdir():
        if path.is_file():
            write(SKETCH / path.name, path.read_bytes())
    prepare_languages(ROOT, SKETCH, write)
    write(SKETCH / "release_config.h", config)
    seed = ROOT / ".local/sheets.h"
    write(SKETCH / "seed.h", seed.read_bytes() if not clean and seed.exists() else "#pragma once\n")
    write(SKETCH / "dice_tiles.inc",dice_tiles())
    write(SKETCH / "spin_frames.inc",",\n".join(lvgl_frames()))
    dice = (ROOT / "docs/screens/art/dice.jpg").read_bytes()
    write(SKETCH / "dice_face.inc", ",".join(map(str,dice)))
    write(SKETCH / "spell_face.inc", ",".join(map(str,(ROOT/"docs/screens/art/spell.jpg").read_bytes())))
    write(SKETCH / "boot_face.inc", ",".join(map(str,(ROOT/"docs/screens/art/dragon-eye.jpg").read_bytes())))
    art = (ROOT / "docs/screens/art/save.jpg").read_bytes()
    write(SKETCH / "fallback_art.inc", ",\n".join(",".join(map(str, art[i:i+32])) for i in range(0, len(art), 32)))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--version", default=(ROOT / "firmware/VERSION").read_text().strip())
    parser.add_argument("--feed-url", default=release.FEED)
    parser.add_argument("--test-ca", type=Path, help="Local HTTPS testing on a development build")
    parser.add_argument("--release-note", help="Build without private seeds and package the app and manifest")
    parser.add_argument("--ota-from", action="append", default=[], help="Qualified previous version for reversible OTA (repeatable)")
    parser.add_argument("--image-url", help="Permanent HTTPS URL for this release's app image")
    args = parser.parse_args()
    clean = args.release_note is not None
    if clean and (args.test_ca or args.prepare_only):
        parser.error("A release requires a compiled image and the built-in trust store.")
    if args.ota_from and not clean:
        parser.error("--ota-from requires --release-note")
    if args.image_url and not clean:
        parser.error("--image-url requires --release-note")
    prepare(release.configure(args.version, args.feed_url, clean, args.test_ca), clean)
    if args.prepare_only:
        return
    cli = os.environ.get("ARDUINO_CLI") or shutil.which("arduino-cli") or "/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
    installed = json.loads(subprocess.check_output([cli, "core", "list", "--json"]))
    if not any(p["id"] == "esp32:esp32" and p["installed_version"] == "4.0.0" for p in installed["platforms"]):
        sys.exit("Install esp32:esp32@4.0.0 with Arduino Boards Manager.")
    subprocess.run([cli, "compile", "--fqbn", FQBN, "--build-path", BUILD / "objects",
                    "--build-property", f"compiler.c.extra_flags=-DLV_CONF_INCLUDE_SIMPLE -I{LIBS}",
                    "--build-property", f"compiler.cpp.extra_flags=-DLV_CONF_INCLUDE_SIMPLE -I{LIBS}",
                    "--libraries", LIBS, "--output-dir", BUILD / "bin", SKETCH], check=True)

    if clean:
        version = args.version
        url = args.image_url or f"{release.BASE}/download/v{version}/{release.ASSET}"
        output = BUILD / "releases" / version
        release.package(BUILD / "bin/Companion.ino.bin", version, url, args.release_note, output, args.ota_from)
        print(f"Release files in {output}")

if __name__ == "__main__":
    main()
