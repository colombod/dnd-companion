"""Read and verify an ESP32-S3 recovery image before its first picker upload."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--esptool", required=True)
    ap.add_argument("--port", required=True)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    if args.output.exists():
        ap.error("Choose a new recovery filename.")
    os.umask(0o077)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [args.esptool, "--chip", "esp32s3", "--port", args.port, "--no-stub"]
    size = 16 * 1024 * 1024
    with tempfile.TemporaryDirectory(dir=args.output.parent) as stage:
        partial = Path(stage) / "image.bin"
        chunk = Path(stage) / "part.bin"
        with partial.open("wb") as out:
            for offset in range(0, size, 512 * 1024):
                for attempt in range(3):
                    read = cmd + ["--after", "no-reset", "read-flash", "--no-progress",
                                  hex(offset), "0x80000", str(chunk)]
                    run = subprocess.run(read, capture_output=True, text=True)
                    if run.returncode == 0 and chunk.stat().st_size == 512 * 1024:
                        break
                    if attempt == 2:
                        raise SystemExit(run.stdout + run.stderr)
                out.write(chunk.read_bytes())
                print(f"Read {(offset + chunk.stat().st_size) // 1024} KiB", flush=True)
        subprocess.run(cmd + ["verify-flash", "0", str(partial)], check=True)
        os.link(partial, args.output)
    print("Verified SHA-256", hashlib.sha256(args.output.read_bytes()).hexdigest())

if __name__ == "__main__":
    main()
