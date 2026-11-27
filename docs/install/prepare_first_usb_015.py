"""Prepare the four USB first-install files for the distributed 0.1.5 Waveshare dial."""
import argparse
import hashlib
from pathlib import Path
import struct
import tempfile
import zlib


# These inputs belong to the 0.1.5 application and its qualified Arduino 4.0.0 build.
PINS = {
    "Companion.ino.bootloader.bin": "57be7946f0ada18ae40cb1fa1e6794ac903bd97ccc0de06e0291fd2a93431e84",
    "Companion.ino.partitions.bin": "ace02503447d0f470692e65fa76002f2d77a92dc81cd3813d8aa66718d716da9",
    "boot_app0.bin": "f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0",
    "Companion.ino.bin": "7c7d387c30f4f97753b836a22d9ca3e30af01b1a9d7bc7c49ffd2cc1cf182879",
}
FLASH_ARGS = """--flash-mode dio --flash-freq 80m --flash-size 16MB
0x0 Companion.ino.bootloader.bin
0x8000 Companion.ino.partitions.bin
0xe000 boot_app0.bin
0x10000 Companion.ino.bin
"""


def prepare(objects, app, output):
    files = {}
    for name, digest in PINS.items():
        source = app if name == "Companion.ino.bin" else objects / name
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f"Unexpected {name}. Use the 0.1.5 release and its pinned ESP32 core.")
        files[name] = data

    # IDF esp_ota_select_entry_t. Keep the sequence and its CRC. NEW makes the
    # rollback-enabled bootloader request the firmware's normal first-boot health check.
    selector = bytearray(files["boot_app0.bin"])
    seq, = struct.unpack_from("<I", selector)
    state, crc = struct.unpack_from("<II", selector, 24)
    if seq != 1 or state != 0xffffffff or crc != zlib.crc32(selector[:4], 0xffffffff):
        raise ValueError("Unexpected factory boot selector.")
    struct.pack_into("<I", selector, 24, 0)  # ESP_OTA_IMG_NEW
    selector[4096:] = b"\xff" * 4096  # No previous Companion on a factory unit.
    files["boot_app0.bin"] = selector
    files["flash_args"] = FLASH_ARGS.encode("ascii")

    if output.exists():
        if (output.is_dir() and {p.name for p in output.iterdir()} == files.keys()
                and all((output / name).read_bytes() == data for name, data in files.items())):
            return
        raise ValueError(f"Choose an empty destination path. {output} already exists.")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
        staging = Path(temporary) / "kit"
        staging.mkdir()
        for name, data in files.items():
            (staging / name).write_bytes(data)
        staging.rename(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--objects", type=Path, default=Path(".build/objects"))
    parser.add_argument("--app", type=Path, required=True, help="The downloaded 0.1.5 release app")
    parser.add_argument("--output", type=Path, default=Path(".build/first-usb-0.1.5"))
    args = parser.parse_args()
    try:
        prepare(args.objects, args.app, args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Prepared {args.output} for first installation. Firmware 0.1.5, build 95a184a82.")


if __name__ == "__main__":
    main()
