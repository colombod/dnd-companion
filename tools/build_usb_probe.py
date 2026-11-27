"""Build the static USB connection checker in .build/usb-site."""
import hashlib
import shutil
import tarfile
from io import BytesIO
from pathlib import Path
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = "esptool-js-0.7.1.tgz"
DIGEST = ("bf0d62b13e51387829466306df2e81f3575c79e67b3b55a1175c74e6a15b0251"
          "fc37437fa4e42a383cc92c0fb7ea509b3970b0d3a880e0259cb72ad043b55e48")


def build_site(output):
    build = ROOT / ".build"
    build.mkdir(exist_ok=True)
    cached = build / PACKAGE
    if cached.exists():
        data = cached.read_bytes()
    else:
        with urlopen(f"https://registry.npmjs.org/esptool-js/-/{PACKAGE}", timeout=30) as response:
            data = response.read()
    if hashlib.sha512(data).hexdigest() != DIGEST:
        raise SystemExit(f"Integrity check failed for {PACKAGE}")
    if not cached.exists():
        cached.write_bytes(data)

    output.mkdir(exist_ok=True)
    with tarfile.open(fileobj=BytesIO(data), mode="r:gz") as archive:
        for member, filename in [("bundle.js", "esptool.js"), ("LICENSE", "esptool-LICENSE")]:
            with archive.extractfile("package/" + member) as source:
                (output / filename).write_bytes(source.read())
    for name in ["index.html", "probe.mjs", "site.mjs", "update.mjs", "style.css", "dial-end.svg"]:
        shutil.copyfile(ROOT / "usb" / name, output / name)
    shutil.copyfile(ROOT / "docs/screens/art/dragon-eye.jpg", output / "dragon-eye.jpg")
    print(f"USB connection checker built in {output}")


if __name__ == "__main__":
    build_site(ROOT / ".build/usb-site")
