"""Provision an existing character's artwork over the USB diagnostics port."""
import argparse
import io
import json
import os
from pathlib import Path
import select
import termios
import time
import tty
from urllib.parse import urlsplit

from PIL import Image, ImageDraw, ImageOps
from seed_picker import fetch


def profile_icon(character):
    link = f"https://character-service.dndbeyond.com/character/v5/character/{character}"
    record = json.loads(fetch(link, 2 * 1024 * 1024))["data"]
    if record["id"] != character:
        raise ValueError("Character ID differs from the requested sheet")
    url = record["decorations"]["avatarUrl"]
    target = urlsplit(url)
    if target.scheme != "https" or target.hostname not in {
        "www.dndbeyond.com", "media.dndbeyond.com", "imgsrv.dndbeyond.com"
    }:
        raise ValueError("No supported D&D Beyond icon URL")
    fingerprint = 2166136261
    for byte in url.encode():
        fingerprint = ((fingerprint ^ byte) * 16777619) & 0xffffffff
    return Image.open(io.BytesIO(fetch(url, 4 * 1024 * 1024))), fingerprint or 1


def prepare(image, side, limit):
    image = ImageOps.fit(ImageOps.exif_transpose(image).convert("RGB"), (side, side))
    mask = Image.new("L", image.size)
    ImageDraw.Draw(mask).ellipse((0, 0, side-1, side-1), fill=255)
    square = Image.new("RGB", image.size, "#111521")
    square.paste(image, mask=mask)
    for quality in (86, 76, 66, 54, 42, 30):
        output = io.BytesIO()
        square.save(output, "JPEG", quality=quality, optimize=True, progressive=False)
        if output.tell() <= limit:
            return output.getvalue()
    raise ValueError("Cropped image exceeds the device limit")


def expect(fd, wanted):
    data = bytearray()
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        if select.select([fd], [], [], .25)[0]:
            data.extend(os.read(fd, 8192))
            if wanted in data:
                return
            if any(msg in data for msg in (b"ART failed", b"ART invalid", b"ART no memory")):
                break
    raise RuntimeError("Device did not confirm " + wanted.decode() + ": " + data[-200:].decode(errors="replace"))


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--port", required=True)
    cli.add_argument("character", type=int)
    cli.add_argument("layer", choices=["icon","background"])
    cli.add_argument("file", type=Path, nargs="?")
    args = cli.parse_args()
    if args.layer == "icon":
        if args.file:
            cli.error("Icons are fetched from the linked character")
        image, source = profile_icon(args.character)
        data = prepare(image, 128, 12288)
    else:
        if not args.file:
            cli.error("Choose a background file")
        source = 0
        data = prepare(Image.open(args.file), 360, 40960)
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    original = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        termios.tcflush(fd, termios.TCIFLUSH)
        os.write(fd, f"ART {args.character} {args.layer} {len(data)} {source}\n".encode())
        expect(fd, b"ART ready")
        view = memoryview(data)
        while view:
            if not select.select([], [fd], [], 3)[1]:
                raise TimeoutError("Device stopped receiving image")
            # The native USB receiver has a 256-byte FIFO in this core.
            view = view[os.write(fd, view[:64]):]
            time.sleep(.004)
        expect(fd, b"ART saved")
        print(f"{args.character} {args.layer}: {len(data)} bytes saved on dial")
    finally:
        termios.tcsetattr(fd, termios.TCSANOW, original)
        os.close(fd)


if __name__ == "__main__":
    main()
