import tempfile
import unittest
from pathlib import Path

from core.bus import Bus
from core.cpu import Cpu
from core.loader import LoadError, LoadSpec, apply_loads, parse_load
from core.module import Module
from core.profile import AliasSpec, MemorySpec, Profile, ModuleSpec


class FakeFlash(Module):
    loadable = True

    def __init__(self, name):
        super().__init__(name)
        self.loaded = []

    def check_load(self, offset, size):
        if size % 34:
            raise ValueError("%d bytes is not a multiple of 34" % size)

    def ranges(self):
        return [("xip", 0x1000)]

    def load(self, offset, data):
        self.loaded.append((offset, bytes(data)))


def tmpfile(data):
    path = Path(tempfile.mkdtemp()) / "image.bin"
    path.write_bytes(data)
    return path


class LoaderTest(unittest.TestCase):
    def setUp(self):
        self.cpu = Cpu()
        self.bus = Bus(self.cpu)
        self.bus.add_memory("RAM1", 0x00400000, 0x1000)
        self.bus.add_memory("RAM2", 0x00900000, 0x1000)
        self.flash = FakeFlash("FLASH")
        self.bus.add_module(self.flash, {"xip": 0x00000000})
        self.profile = Profile(
            name="TEST",
            insns_per_us=100,
            peripheral_window=(0x00800000, 0x100000),
            memory=[MemorySpec("RAM1", 0x00400000, 0x1000),
                    MemorySpec("RAM2", 0x00900000, 0x1000)],
            aliases={
                "IRAM": AliasSpec("IRAM", address=0x00900000),
                "APP": AliasSpec("APP", module="FLASH", offset=0x11000),
            },
            modules=[ModuleSpec("flash", "FLASH", {"xip": 0})],
        )
        self.modules = {"FLASH": self.flash}

    def load(self, target, data):
        spec = LoadSpec(target, tmpfile(data))
        apply_loads([spec], self.profile, self.bus, self.cpu, self.modules)

    def test_parse_load_splits_on_first_colon(self):
        spec = parse_load("app:build/app_crc.bin")
        self.assertEqual((spec.target, str(spec.path)), ("app", "build/app_crc.bin"))

    def test_parse_load_without_colon_rejected(self):
        with self.assertRaisesRegex(LoadError, "target:file"):
            parse_load("nofile")

    def test_alias_into_ram_case_insensitive(self):
        self.load("iram", b"\x01\x02\x03\x04")
        self.assertEqual(self.cpu.mem_read(0x00900000, 4), b"\x01\x02\x03\x04")

    def test_numeric_into_ram(self):
        self.load("0x00400010", b"\xAA")
        self.assertEqual(self.cpu.mem_read(0x00400010, 1), b"\xAA")

    def test_outside_memory_rejected(self):
        with self.assertRaisesRegex(LoadError, "0x20000000"):
            self.load("0x20000000", b"\x00")

    def test_crossing_region_end_rejected(self):
        with self.assertRaisesRegex(LoadError, "does not fit"):
            self.load("0x00400FFF", b"\x00\x00")

    def test_file_ending_exactly_at_region_end_ok(self):
        self.load("0x00400FFC", b"\x01\x02\x03\x04")
        self.assertEqual(self.cpu.mem_read(0x00400FFC, 4), b"\x01\x02\x03\x04")

    def test_into_mmio_range_rejected(self):
        with self.assertRaisesRegex(LoadError, "FLASH"):
            self.load("0x00000100", b"\x00")

    def test_flash_alias_calls_module_load_with_offset(self):
        self.load("app", b"\x11" * 34)
        self.assertEqual(self.flash.loaded, [(0x11000, b"\x11" * 34)])

    def test_missing_file_rejected(self):
        spec = LoadSpec("iram", Path("/nonexistent/file.bin"))
        with self.assertRaisesRegex(LoadError, "file.bin"):
            apply_loads([spec], self.profile, self.bus, self.cpu, self.modules)

    def test_empty_file_rejected(self):
        with self.assertRaisesRegex(LoadError, "empty"):
            self.load("iram", b"")

    def test_module_rejection_is_a_load_error_naming_the_file(self):
        with self.assertRaisesRegex(LoadError, "image.bin.*100"):
            self.load("app", b"\x00" * 100)

    def test_nothing_applied_when_a_later_module_load_is_bad(self):
        good = LoadSpec("iram", tmpfile(b"\x01"))
        bad = LoadSpec("app", tmpfile(b"\x00" * 100))
        with self.assertRaises(LoadError):
            apply_loads([good, bad], self.profile, self.bus, self.cpu, self.modules)
        self.assertEqual(self.cpu.mem_read(0x00900000, 1), b"\x00")
        self.assertEqual(self.flash.loaded, [])

    def test_nothing_applied_when_a_later_load_is_bad(self):
        good = LoadSpec("iram", tmpfile(b"\x01"))
        bad = LoadSpec("0x20000000", tmpfile(b"\x01"))
        with self.assertRaises(LoadError):
            apply_loads([good, bad], self.profile, self.bus, self.cpu, self.modules)
        self.assertEqual(self.cpu.mem_read(0x00900000, 1), b"\x00")


if __name__ == "__main__":
    unittest.main()
