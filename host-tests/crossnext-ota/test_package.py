import importlib.util
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("package_crossnext", ROOT / "tools_local/package_crossnext.py")
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class ClassicPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.app = bytearray(4096)
        self.app[0] = 0xE9
        struct.pack_into("<H", self.app, 12, 9)
        tag = b"CROSSPOINT-BOARD-V1:x4c;"
        self.app[32:32 + len(tag)] = tag
        self.parts = b"".join(struct.pack("<HBBII16sI", 0x50AA, 0, 16 + i, offset, 4096, f"app{i}".encode(), 0)
                              for i, offset in enumerate((0x10000, 0x20000)))
        self.boot = b"current-build-bootloader"
        self.boot_app = bytes(range(256)) * 32
        self.boot_app_path = self.path / "framework/tools/partitions/boot_app0.bin"
        self.boot_app_path.parent.mkdir(parents=True)
        self.boot_app_path.write_bytes(self.boot_app)
        self.save()

    def save(self):
        factory = bytearray(0x10000 + len(self.app))
        factory[:len(self.boot)] = self.boot
        factory[0x8000:0x8000 + len(self.parts)] = self.parts
        factory[0xE000:0xE000 + len(self.boot_app)] = self.boot_app
        factory[0x10000:] = self.app
        for name, data in (("firmware.bin", self.app), ("firmware.factory.bin", factory),
                           ("bootloader.bin", self.boot), ("partitions.bin", self.parts)):
            (self.path / name).write_bytes(data)

    def test_current_build_identity_fit_and_producers(self):
        result = package.classic_inputs(self.path, self.boot_app_path)
        self.assertEqual(result["board"], "x4c")
        self.assertEqual(len(result["generated_slots"]), 2)
        self.assertIn("unknown", result["installed_table_fit"])
        self.assertEqual(len(result["build_inputs"]), 5)
        digest = hashlib.sha256(self.boot_app).hexdigest()
        self.assertEqual(result["build_inputs"]["boot_app0.bin"], digest)
        self.assertEqual(result["boot_app0"]["source"], str(self.boot_app_path.resolve()))
        self.assertEqual(result["boot_app0"]["offset"], 0xE000)
        self.assertEqual(result["boot_app0"]["sha256"], digest)

    def test_wrong_chip_rejected(self):
        struct.pack_into("<H", self.app, 12, 5)
        self.save()
        with self.assertRaisesRegex(ValueError, "ESP32-S3"):
            package.classic_inputs(self.path, self.boot_app_path)

    def test_wrong_board_rejected(self):
        self.app[32:55] = b"CROSSPOINT-BOARD-V1:x4p;"
        self.save()
        with self.assertRaisesRegex(ValueError, "x4c board tag"):
            package.classic_inputs(self.path, self.boot_app_path)

    def test_generated_slot_overflow_rejected(self):
        self.app.extend(b"x")
        self.save()
        with self.assertRaisesRegex(ValueError, "slot"):
            package.classic_inputs(self.path, self.boot_app_path)

    def test_stale_factory_rejected(self):
        factory = self.path / "firmware.factory.bin"
        image = bytearray(factory.read_bytes())
        image[-1] ^= 1
        factory.write_bytes(image)
        with self.assertRaisesRegex(ValueError, "this build's application"):
            package.classic_inputs(self.path, self.boot_app_path)

    def test_missing_factory_boot_app_rejected(self):
        factory = self.path / "firmware.factory.bin"
        image = bytearray(factory.read_bytes())
        image[0xE000:0x10000] = b"\xff" * len(self.boot_app)
        factory.write_bytes(image)
        with self.assertRaisesRegex(ValueError, "boot_app0"):
            package.classic_inputs(self.path, self.boot_app_path)

    def test_corrupt_factory_boot_app_rejected(self):
        factory = self.path / "firmware.factory.bin"
        image = bytearray(factory.read_bytes())
        image[0xE000] ^= 1
        factory.write_bytes(image)
        with self.assertRaisesRegex(ValueError, "boot_app0"):
            package.classic_inputs(self.path, self.boot_app_path)

    def test_missing_framework_boot_app_rejected(self):
        self.boot_app_path.unlink()
        with self.assertRaises(FileNotFoundError):
            package.classic_inputs(self.path, self.boot_app_path)

    def test_empty_framework_boot_app_rejected(self):
        self.boot_app_path.write_bytes(b"")
        with self.assertRaisesRegex(ValueError, "boot_app0"):
            package.classic_inputs(self.path, self.boot_app_path)


if __name__ == "__main__":
    unittest.main()
