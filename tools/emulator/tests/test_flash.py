import struct
import unittest

from core.module import Fault
from modules.flash import FlashModule, crc16

CRC_EN = 1 << 26
CONF = 0x1C  # register index 7
OPERATE = 0x00
FIFO_READ = 0x08
JEDEC = 0x10


def wrap(data32):
    return data32 + struct.pack(">H", crc16(data32))


def image(*blocks):
    return b"".join(wrap(b) for b in blocks)


def block(fill):
    return bytes([fill]) * 32


class FlashTest(unittest.TestCase):
    def setUp(self):
        self.flash = FlashModule("FLASH", size=0x200000)

    def test_crc16_vectors(self):
        self.assertEqual(crc16(bytes(32)), 0x8029)
        self.assertEqual(crc16(bytes(range(32))), 0x259D)
        self.assertEqual(crc16(b"\xff" * 32), 0x000C)

    def test_ranges_cover_last_valid_logical_address(self):
        sizes = dict(self.flash.ranges())
        self.assertGreaterEqual(sizes["xip"], 0x1E1E00)
        self.assertEqual(sizes["xip"] % 0x1000, 0)

    def test_good_block_reads_decoded(self):
        self.flash.load(0, image(bytes(range(32))))
        self.assertEqual(self.flash.read("xip", 0, 4), 0x03020100)
        self.assertEqual(self.flash.read("xip", 28, 4), 0x1F1E1D1C)

    def test_app_offset_maps_to_logical_0x10000(self):
        self.flash.load(0x11000, image(block(1), block(2), block(3)))
        data, bad = self.flash.page("xip", 0x10000)
        self.assertEqual(data[:96], block(1) + block(2) + block(3))
        self.assertEqual(bad, [(96, 0x1000)])

    def test_fully_good_page_has_no_bad_spans(self):
        self.flash.load(0, image(*[block(i) for i in range(128)]))
        _, bad = self.flash.page("xip", 0)
        self.assertEqual(bad, [])

    def test_bad_block_alerts_and_faults(self):
        self.flash.load(0, image(block(1)) + b"\x00" * 34)
        with self.assertLogs("emulator.flash", level="ERROR") as logs:
            with self.assertRaises(Fault) as ctx:
                self.flash.read("xip", 0x20 + 4, 4)
        self.assertEqual((ctx.exception.kind, ctx.exception.addr), ("dabt", 0x24))
        text = logs.output[0]
        for part in ("ALERT", "0x00000024", "block 1", "0x000022"):
            self.assertIn(part, text)

    def test_repeated_reads_of_the_same_bad_block_alert_once(self):
        with self.assertLogs("emulator.flash", level="ERROR") as logs:
            for _ in range(3):
                with self.assertRaises(Fault):
                    self.flash.read("xip", 0, 4)
        self.assertEqual(len(logs.output), 1)

    def test_alert_repeats_after_a_different_block_alerted(self):
        with self.assertLogs("emulator.flash", level="ERROR") as logs:
            for offset in (0, 0x20, 0):
                with self.assertRaises(Fault):
                    self.flash.read("xip", offset, 4)
        self.assertEqual(len(logs.output), 3)

    def test_bad_block_readable_when_crc_disabled(self):
        self.flash.load(0, b"\x05" * 34)
        self.flash.write("ctrl", CONF, 4, 0)
        self.assertEqual(self.flash.read("xip", 0, 4), 0x05050505)
        _, bad = self.flash.page("xip", 0)
        self.assertEqual(bad, [])

    def test_crc_en_defaults_to_enabled(self):
        self.assertTrue(self.flash.read("ctrl", CONF, 4) & CRC_EN)

    def test_changing_crc_en_invalidates_pages(self):
        calls = []
        self.flash.invalidate = lambda *args: calls.append(args)
        self.flash.write("ctrl", CONF, 4, 0)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0][:2], ("xip", 0))

    def test_unwritten_flash_is_bad_block(self):
        with self.assertLogs("emulator.flash", level="ERROR"):
            with self.assertRaises(Fault):
                self.flash.read("xip", 0, 4)

    def test_read_past_last_valid_logical_address_faults(self):
        # 61680 whole 34-byte blocks fit a 2 MiB chip; the last valid byte is 0x1E1DFF.
        self.flash.write("ctrl", CONF, 4, 0)
        self.assertEqual(self.flash.read("xip", 0x1E1DFC, 4), 0xFFFFFFFF)
        with self.assertLogs("emulator.flash", level="ERROR"):
            with self.assertRaises(Fault):
                self.flash.read("xip", 0x1E1E00, 4)

    def test_load_size_not_multiple_of_34_rejected_naming_size(self):
        with self.assertRaisesRegex(ValueError, "100"):
            self.flash.load(0, b"\x00" * 100)

    def test_load_offset_not_block_aligned_rejected(self):
        with self.assertRaisesRegex(ValueError, "34"):
            self.flash.load(1, b"\x00" * 34)

    def test_load_past_flash_end_rejected(self):
        with self.assertRaisesRegex(ValueError, "flash"):
            self.flash.load(0x200000 - 34, b"\x00" * 68)

    def test_load_invalidates_cached_pages(self):
        calls = []
        self.flash.invalidate = lambda *args: calls.append(args)
        self.flash.load(0x11000, image(block(1)))
        self.assertEqual(calls[0][0], "xip")

    def test_rdid_reports_jedec_id(self):
        self.flash.write("ctrl", OPERATE, 4, (20 << 24) | (1 << 29))
        self.assertEqual(self.flash.read("ctrl", JEDEC, 4), 0x1C7015)
        self.assertEqual(self.flash.read("ctrl", OPERATE, 4) >> 31, 0)

    def test_read_command_fills_fifo_with_raw_physical_bytes(self):
        raw = image(bytes(range(32)))
        self.flash.load(0, raw)
        self.flash.write("ctrl", OPERATE, 4, 0 | (5 << 24) | (1 << 29))
        words = [self.flash.read("ctrl", FIFO_READ, 4) for _ in range(8)]
        self.assertEqual(struct.pack("<8I", *words), raw[:32])


if __name__ == "__main__":
    unittest.main()
