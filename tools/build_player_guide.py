"""Build the local preview, or render the published firmware tags for GitHub Pages."""
import argparse
import hashlib
from html import escape
from html.parser import HTMLParser
from pathlib import Path
import re
import shutil
from string import Template
import subprocess
from tempfile import TemporaryDirectory
from urllib.parse import unquote, urlsplit
from urllib.request import urlopen

from build_usb_probe import build_site as build_usb_checker

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / ".build"
REPO = "colombod/dnd-companion"
VERSION = re.compile(r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)")
GUIDE = ROOT / "guide"
ASSET = re.compile(r"!\[[^\]]*\]\((screens/guide/[a-z0-9-]+\.png)\)")
# Reviewed installation errata for firmware whose release tag is already public.
INSTALL_REVISIONS = {"v0.1.5": "refs/tags/guide-v0.1.5-usb.1"}
USB_ZIP = "first-usb-0.1.5.zip"
USB_URL = f"https://github.com/colombod/dnd-companion-firmware/releases/download/v0.1.5/{USB_ZIP}"
USB_SHA256 = "b0f27986f2f9cfd678623dd0c13dd4fe6c5b45b985ed166b99aa1847f0dab9c2"


class PageLinks(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.urls = []

    def handle_starttag(self, tag, attrs):
        for name, value in attrs:
            if name == "id":
                self.ids.add(value)
            if name in {"href", "src"} and value:
                self.urls.append(value)


def check_links(site):
    site = site.resolve()
    pages = {}
    for path in site.rglob("*.html"):
        page_links = PageLinks()
        page_links.feed(path.read_text())
        pages[path] = page_links
    for path, links in pages.items():
        for url in links.urls:
            ref = urlsplit(url)
            if ref.scheme or ref.netloc:
                continue
            target = (path.parent / unquote(ref.path)).resolve() if ref.path else path
            if not target.is_relative_to(site) or not target.is_file():
                raise ValueError(f"Broken link in {path.name}: {url}")
            if ref.fragment and (target not in pages or unquote(ref.fragment) not in pages[target].ids):
                raise ValueError(f"Missing section in {path.name}: {url}")


def content(path, revision):
    if revision is None:
        return (ROOT / path).read_bytes()
    return subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)


def copy_first_usb_zip(directory):
    cached = BUILD / USB_ZIP
    if cached.exists():
        data = cached.read_bytes()
    else:
        with urlopen(USB_URL, timeout=60) as response:
            data = response.read()
    if hashlib.sha256(data).hexdigest() != USB_SHA256:
        raise ValueError(f"Checksum mismatch for {USB_ZIP}")
    (directory / USB_ZIP).write_bytes(data)


def released_tags():
    tags = subprocess.check_output([
        "gh", "api", "--paginate", f"repos/{REPO}/releases",
        "--jq", ".[] | select(.draft == false and .prerelease == false) | .tag_name",
    ], text=True).splitlines()
    tags = [tag for tag in tags if tag.startswith("v") and VERSION.fullmatch(tag[1:])]
    if not tags:
        raise SystemExit("No published stable firmware releases. Use --preview for a local guide.")
    return sorted(tags, key=lambda tag: tuple(map(int, tag[1:].split("."))), reverse=True)


def page(target, prefix, title, body, nav):
    template = Template((GUIDE / "page.html").read_text())
    target.write_text(template.substitute(prefix=prefix, title=escape(title), body=body, nav=nav))


def render_version(output, tag):
    release = f"refs/tags/{tag}" if tag else None
    version = content("firmware/VERSION", release).decode().strip()
    if not VERSION.fullmatch(version) or (tag and tag != "v" + version):
        raise ValueError(f"Firmware version {version!r} does not match {tag!r}")
    path = f"guide/v{version}" if tag else f"preview/v{version}"
    directory = output / path
    directory.mkdir(parents=True)
    label = f"Firmware {version}" if tag else f"Firmware {version} · development preview"
    guides = [
        ("docs/MANUAL.md", "index.html", "Player manual", False),
        ("docs/INSTALL.md", "install.html", "Installation", False),
        ("docs/FEATURES.md", "features.html", "Release features", True),
        ("docs/RULES.md", "rules.html", "Rulebooks", True),
    ]
    pages = {}
    assets = {}
    for source, filename, title, excerpt in guides:
        revision = INSTALL_REVISIONS.get(tag, release) if filename == "install.html" else release
        body = content(source, revision).decode()
        if excerpt:
            sections = re.findall(r"<!-- player-guide:start -->(.*?)<!-- player-guide:end -->", body, re.S)
            if not sections:
                raise ValueError(f"Missing player guide sections in {source}")
            body = "\n\n".join(sections)
        refs = set(ASSET.findall(body))
        if filename == "install.html":
            factory_tool = "install/prepare_first_usb_015.py"
            if f"]({factory_tool})" in body:
                refs.add(factory_tool)
            if f"]({USB_URL})" in body:
                copy_first_usb_zip(directory)
                body = body.replace(f"]({USB_URL})", f"]({USB_ZIP})")
        for asset in refs:
            data = content("docs/" + asset, revision)
            if asset in assets and assets[asset] != data:
                raise ValueError(f"Conflicting versions of {asset} in guide {version}")
            assets[asset] = data
        pages[filename] = (title, body)
    for asset, data in assets.items():
        dest = directory / asset
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
    nav = (f'<span>{label}</span><a href="install.html">Install</a><a href="index.html">Manual</a>'
           '<a href="features.html">Features</a><a href="rules.html">Rulebooks</a>'
           '<a href="../../index.html">All versions</a>')
    for filename, (title, text) in pages.items():
        for source, target, _, _ in guides:
            text = text.replace("](" + Path(source).name, "](" + target)
        html = subprocess.check_output(["pandoc", "-f", "gfm", "-t", "html5", "--wrap=none",
                                        "--toc", "--toc-depth=3", "--template", str(GUIDE / "article.html")],
                                       input=text, text=True)
        page(directory / filename, "../../", f"{title} · {label}", html,
             nav.replace(f'href="{filename}"', f'href="{filename}" aria-current="page"'))
    return path, label


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--preview", action="store_true", help="Read working-tree docs into a private local preview")
    modes.add_argument("--published", action="store_true", help="Read stable GitHub releases and their local tags")
    args = parser.parse_args()
    tags = [None] if args.preview else released_tags()
    BUILD.mkdir(exist_ok=True)
    dest = BUILD / ("player-guide-preview" if args.preview else "player-guide")
    with TemporaryDirectory(dir=BUILD) as temp:
        output = Path(temp) / "site"
        output.mkdir()
        build_usb_checker(output / "usb")
        guides = [render_version(output, tag) for tag in tags]
        first, label = guides[0]
        shutil.copyfile(GUIDE / "guide-hero.webp", output / "guide-hero.webp")
        shutil.copytree(GUIDE / "fonts", output / "fonts")
        (output / "guide.css").write_bytes((GUIDE / "guide.css").read_bytes())
        latest = f"refs/tags/{tags[0]}" if tags[0] else None
        (output / "dragon-eye.jpg").write_bytes(content("docs/screens/art/dragon-eye.jpg", latest))
        links = "".join(f'<li><a href="{url}/index.html">{name}</a></li>' for url, name in guides)
        home = Template((GUIDE / "home.html").read_text()).substitute(first=first, label=label, links=links)
        page(output / "index.html", "", "Player guides", home, '<a href="#at-the-table">At the table</a><a href="#versions">Firmware versions</a>')
        (output / ".nojekyll").touch()
        check_links(output)
        if dest.exists():
            shutil.rmtree(dest)
        output.rename(dest)
    print(f"Built {len(guides)} firmware guide(s) in {dest}")


if __name__ == "__main__":
    main()
