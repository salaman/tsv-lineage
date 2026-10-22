import gzip
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "boot", Path(__file__).resolve().parents[1] / "tools/prepare-boot.py")
boot = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(boot)


class BootTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="tsv-boot-test-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.dtb = self.root / "board.dtb"
        self.dtb.write_bytes(struct.pack(">10I", 0xD00DFEED, 40, 40, 40, 40, 17, 16, 0, 0, 0))
        kernel = bytearray(4096)
        struct.pack_into("<2Q", kernel, 8, 0x80000, len(kernel))
        kernel[56:60] = b"ARM\x64"
        compressed = gzip.compress(kernel, mtime=0) + self.dtb.read_bytes()
        ramdisk = b"\x02\x21\x4c\x18" + bytes(32)
        header = struct.pack("<8s10I", b"ANDROID!", len(compressed), 0x80008000,
                             len(ramdisk), 0x81000000, 0, 0, 0x80000100, 2048, 0, 0)
        self.payload = self.root / "boot.img"
        self.payload.write_bytes(header.ljust(2048, b"\0") + compressed.ljust(2048, b"\0")
                                 + ramdisk.ljust(2048, b"\0"))
        self.backup = self.root / "backup.img"
        header = struct.pack("<8s10I", b"ANDROID!", 4096, 0, 0, 0, 0, 0, 0, 2048, 0, 0)
        with self.backup.open("wb") as stream:
            stream.write(header.ljust(64, b"\0") + b"lk2nd\0")
            stream.truncate(boot.PARTITION_BYTES)
        self.output = self.root / "physical-boot.img"

    def test_preserves_device_prefix_and_payload(self):
        digest = boot.prepare(self.backup, self.payload, self.dtb, self.output)
        self.assertEqual(len(digest), 64)
        self.assertEqual(self.output.stat().st_size, boot.PARTITION_BYTES)
        with self.output.open("rb") as result, self.backup.open("rb") as original:
            self.assertEqual(result.read(boot.PREFIX_BYTES), original.read(boot.PREFIX_BYTES))
            self.assertEqual(result.read(self.payload.stat().st_size), self.payload.read_bytes())

    def test_does_not_replace_existing_output(self):
        self.output.write_bytes(b"personal backup")
        with self.assertRaisesRegex(ValueError, "exists"):
            boot.prepare(self.backup, self.payload, self.dtb, self.output)
        self.assertEqual(self.output.read_bytes(), b"personal backup")

    def test_wrong_dtb_is_rejected(self):
        self.dtb.write_bytes(b"wrong board")
        with self.assertRaisesRegex(ValueError, "matching ThinkSmart"):
            boot.prepare(self.backup, self.payload, self.dtb, self.output)
        self.assertFalse(self.output.exists())

    def test_non_lk2nd_backup_is_rejected(self):
        with self.backup.open("r+b") as stream:
            stream.write(b"not lk2nd")
        with self.assertRaisesRegex(ValueError, "recognized installed"):
            boot.prepare(self.backup, self.payload, self.dtb, self.output)

    def test_backup_requires_full_physical_partition(self):
        with self.backup.open("r+b") as stream:
            stream.truncate(boot.PARTITION_BYTES - 1)
        with self.assertRaisesRegex(ValueError, "full physical"):
            boot.prepare(self.backup, self.payload, self.dtb, self.output)
        self.assertFalse(self.output.exists())

    def test_payload_cannot_consume_lk2nd_reservation(self):
        with self.assertRaisesRegex(ValueError, "does not fit"):
            boot.validate_payload(bytes(boot.PARTITION_BYTES - boot.PREFIX_BYTES + 1),
                                  self.dtb.read_bytes())

    def test_kernel_boot_window_exact_boundary(self):
        kernel = bytearray(4096)
        text_offset = 0x80000
        kernel[56:60] = b"ARM\x64"
        original = self.payload.read_bytes()
        for extra in (0, 1):
            struct.pack_into("<2Q", kernel, 8, text_offset,
                             50 * 1024 * 1024 - text_offset + extra)
            compressed = gzip.compress(kernel, mtime=0) + self.dtb.read_bytes()
            header = bytearray(original[:2048])
            struct.pack_into("<I", header, 8, len(compressed))
            data = header + compressed.ljust(2048, b"\0") + original[4096:]
            if extra:
                with self.assertRaisesRegex(ValueError, "reserved boot window"):
                    boot.validate_payload(data, self.dtb.read_bytes())
            else:
                boot.validate_payload(data, self.dtb.read_bytes())


if __name__ == "__main__":
    unittest.main()
