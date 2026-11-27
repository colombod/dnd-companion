"""Check update packaging and its version/board/size contracts."""
from pathlib import Path
from tempfile import TemporaryDirectory
import hashlib
import json
import unittest

import firmware_release as release


class ReleaseTests(unittest.TestCase):
    def test_versions_and_transport(self):
        for v in ("0.1.0", "1.10.0", "9999.9999.9999"):
            self.assertTrue(release.valid_version(v))
        for v in ("v1.0.0", "0.01.0", "1.0", "0.1.0-beta", "10000.0.0"):
            self.assertFalse(release.valid_version(v))
        for url in ("http://a/file", "https://u:p@host/a", "https://", "https://host/a\nb"):
            self.assertFalse(release.https_url(url))

    def test_offer_lists_only_explicit_older_starting_versions(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            app = root / "app.bin"
            app.write_bytes(b"\xe9" + f"DIAL/{release.TARGET}/release/0.1.4\0".encode())
            dest = root / "release"
            def package(previous):
                release.package(app, "0.1.4", "https://host/app.bin", "Restore", dest, previous)
                return json.loads((dest / "companion.json").read_text())["otaFrom"]
            self.assertEqual(package([]), [])
            self.assertEqual(package(["0.1.3", "0.1.3"]), ["0.1.3"])
            for invalid in (["0.1.4"], ["0.1.12"], ["0.01.3"]):
                with self.assertRaises(ValueError):
                    package(invalid)

    def test_only_release_app_image(self):
        version = "0.2.1"
        with TemporaryDirectory() as tmp:
            folder = Path(tmp)
            binary = folder / "app.bin"
            output = folder / "release"
            identity = f"DIAL/{release.TARGET}/release/{version}\0".encode()
            valid = b"\xe9" + bytes(200) + identity
            for invalid in (valid[1:], valid.replace(b"release", b"dev"), valid.replace(b"0.2.1", b"0.2.10"),
                            valid.replace(b"waveshare", b"anotherxx"), valid + bytes(release.SLOT_SIZE)):
                binary.write_bytes(invalid)
                with self.assertRaises(ValueError):
                    release.package(binary, version, "https://host/app.bin", "Player change", output)
                self.assertFalse(output.exists())
            binary.write_bytes(valid)
            release.package(binary, version, "https://host/app.bin", "Player change", output)
            manifest = json.loads((output / "companion.json").read_text())
            self.assertEqual(manifest["format"], 2)
            self.assertEqual(manifest["sha256"], hashlib.sha256(valid).hexdigest())
            self.assertEqual(manifest["size"], len(valid))
            self.assertEqual(manifest["target"], release.TARGET)
            self.assertEqual((output / release.ASSET).read_bytes(), valid)


if __name__ == "__main__":
    unittest.main()
