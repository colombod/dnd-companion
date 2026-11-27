"""Exercise a saved session through old -> candidate -> old firmware on the host."""
import argparse
import io
import os
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def build(ref, directory):
    revision = git("rev-parse", "--verify", "--end-of-options", ref + "^{commit}").decode().strip()
    version = git("show", revision + ":firmware/VERSION").decode().strip()
    directory.mkdir()
    folder = PurePosixPath("firmware/Companion")
    data = git("archive", revision, str(folder))
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        for member in archive:
            path = PurePosixPath(member.name)
            if member.isfile() and path.parent == folder and (path.suffix == ".h" or path.name == "session_store.cpp"):
                with archive.extractfile(member) as source:
                    (directory / path.name).write_bytes(source.read())
    exe = directory / "check"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-I", str(directory),
                    "-I", str(ROOT / "tools/test_support"), str(ROOT / "tools/test_support/restore_pair.cpp"),
                    str(directory / "session_store.cpp"), "-o", str(exe)], check=True)
    return exe, f"{version} ({revision[:12]})"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("previous", help="Git revision for the installed firmware")
    parser.add_argument("candidate", help="Git revision for the proposed update")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as scratch:
        tmp = Path(scratch)
        previous, old = build(args.previous, tmp / "previous")
        candidate, new = build(args.candidate, tmp / "candidate")
        state = tmp / "saves"
        state.mkdir()
        for exe, label, step in [(previous, old, "seed"), (candidate, new, "play"),
                                 (previous, old, "restore"), (previous, old, "reopen")]:
            print(f"{step} with {label}", flush=True)
            result = subprocess.run([str(exe), step], cwd=state)
            if result.returncode:
                raise SystemExit(f"FAIL at {step} with {label}")
        print("PASS: both synthetic sessions survived, including the new cast and Undo.")


if __name__ == "__main__":
    main()
