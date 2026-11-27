"""Check illustrated device faces against the limits in docs/HARDWARE.md."""
from pathlib import Path
from struct import unpack
import sys

ART = Path(__file__).resolve().parents[1] / "docs/screens/art"
DISPLAY = (360, 360)
MAX_BYTES = 40 * 1024


def check_face(path):
    data = path.read_bytes()
    if len(data) > MAX_BYTES:
        raise ValueError(f"{len(data)} bytes exceeds the {MAX_BYTES}-byte face budget")
    if not data.startswith(b"\xff\xd8") or not data.endswith(b"\xff\xd9"):
        raise ValueError("expected a complete JPEG")
    pos = 2
    while pos + 4 <= len(data) and data[pos] == 0xFF:
        marker = data[pos + 1]
        length = int.from_bytes(data[pos + 2:pos + 4], "big")
        if length < 2 or pos + 2 + length > len(data):
            break
        if marker == 0xC0:
            if length < 8:
                raise ValueError("truncated JPEG frame header")
            depth, height, width, components = unpack(">BHHB", data[pos + 4:pos + 10])
            if (width, height) != DISPLAY or depth != 8 or components != 3:
                raise ValueError(f"expected 8-bit three-component {DISPLAY}, got {width}x{height}, {depth} bits, {components} components")
            return len(data)
        if marker in (0xC2, 0xDA):
            break
        pos += 2 + length
    raise ValueError("baseline JPEG frame header missing")


def main():
    paths = sorted(ART.iterdir())
    if not {"dice.jpg", "save.jpg", "spell.jpg", "dragon-eye.jpg"}.issubset(p.name for p in paths):
        raise ValueError("a play or boot illustration is missing")
    total = 0
    for path in paths:
        try:
            size = check_face(path)
        except (OSError, ValueError) as error:
            raise ValueError(f"{path.name}: {error}") from error
        print(f"{path.name:12} 360x360  {size:6} bytes")
        total += size
    sprite_path = ART.parent / "dice-tokens.png"
    sprite = sprite_path.read_bytes()
    if sprite[:8] != b"\x89PNG\r\n\x1a\n" or sprite[12:16] != b"IHDR" or len(sprite) < 33:
        raise ValueError("expected a PNG dice atlas with an IHDR header")
    width, height, depth, color = unpack(">IIBB", sprite[16:26])
    if (width, height, depth, color) != (7 * 52, 52, 8, 3) or len(sprite) > 12 * 1024:
        raise ValueError("expected seven 52px dice in an 8-bit indexed atlas within 12 KiB")
    print(f"{sprite_path.name:12} {width}x{height}  {len(sprite):6} bytes")
    print(f"{total + len(sprite)} stored bytes. One decoded RGB565 face uses 259200 bytes.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        sys.exit(str(error))
