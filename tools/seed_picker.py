"""Prepare an optional private offline character summary for the picker."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parents[1]

def fetch(url, max_bytes):
    return subprocess.check_output(["curl", "-fLsS", "--max-time", "30", "--max-filesize", str(max_bytes),
                                    "--proto", "=https", "--proto-redir", "=https", url])

def prepare(link):
    u = urlparse(link)
    match = re.fullmatch(r"/characters/([1-9][0-9]*)(?:/[\w-]+)?/?", u.path)
    if u.scheme != "https" or u.hostname not in ("dndbeyond.com", "www.dndbeyond.com") or not match:
        raise ValueError("Use a D&D Beyond character link.")
    cid = int(match[1])
    payload = json.loads(fetch(f"https://character-service.dndbeyond.com/character/v5/character/{cid}", 2*1024*1024))
    d = payload["data"]
    if not payload["success"] or d["id"] != cid:
        raise ValueError("Wrong source character")
    classes = " / ".join(f"{c['definition']['name']} {c['level']}" for c in d["classes"])
    level = sum(c["level"] for c in d["classes"])
    if not (0 < cid < 2**32 and 0 < level <= 20 and 0 < len(d["name"].encode()) < 64 and len(classes.encode()) < 120):
        raise ValueError("Character summary exceeds the device format")
    now = int(datetime.now(timezone.utc).timestamp())
    url = (d.get("decorations") or {}).get("avatarUrl") or ""
    revision = d.get("dateModified") or ""
    fields = [str(cid), json.dumps(d["name"],ensure_ascii=False), json.dumps(classes,ensure_ascii=False),str(now)+"u",str(level),
              '{"", "", '+json.dumps(revision if len(revision)<40 else "")+'}',json.dumps(url if len(url)<400 else "")]
    print(f"Prepared {d['name']}, {classes}")
    return cid, "{"+", ".join(fields)+"}"

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("links",nargs="+")
    args=ap.parse_args()
    profiles=dict(prepare(link) for link in args.links)
    if len(profiles)>12: ap.error("The dial holds 12 characters")
    entries=["{"+sheet+"}" for sheet in profiles.values()]
    text=["#pragma once", "#define HAS_SEEDS 1", "static const PrivateSeed seeds[] = {"+",\n".join(entries)+"};"]
    target=ROOT/".local/sheets.h";target.parent.mkdir(exist_ok=True)
    tmp=target.with_suffix(".tmp");tmp.write_text("\n".join(text)+"\n");tmp.replace(target)

if __name__ == "__main__":
    main()
