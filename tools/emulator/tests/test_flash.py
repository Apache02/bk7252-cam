import logging
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

    def test_reset_restores_registers_but_keeps_contents(self):
        calls = []
        self.flash.load(0, image(bytes(range(32))))
        self.flash.write("ctrl", CONF, 4, 0)
        self.flash.invalidate = lambda *args: calls.append(args)
        self.flash.reset()
        self.assertTrue(self.flash.read("ctrl", CONF, 4) & CRC_EN)
        self.assertEqual(len(calls), 1)
        self.assertEqual(self.flash.read("xip", 0, 4), 0x03020100)

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

    def test_load_of_any_size_overwrites_only_its_bytes_and_leaves_the_rest_erased(self):
        self.flash.load(0x11000, b"\x00" * 100)
        raw = self.flash._raw
        self.assertEqual(bytes(raw[0x11000:0x11064]), b"\x00" * 100)
        self.assertEqual(bytes(raw[0x11064:0x11068]), b"\xff" * 4)

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


START, WP = 1 << 29, 1 << 30
WREN, WRDI, RDSR, WRSR, READ, RDSR2, WRSR2, PP, SE, BE1, BE2, CE = 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15, 16
FIFO_WRITE, SR_REG = 0x04, 0x14
SR_WEL = 1 << 1


class WriteTest(unittest.TestCase):
    """Erase, program and the write-protection model of docs/hardware/flash.md."""

    def setUp(self):
        logger = logging.getLogger("emulator.flash")
        quiet = logging.NullHandler()  # keeps expected warnings off the test output
        logger.addHandler(quiet)
        self.addCleanup(logger.removeHandler, quiet)
        self.flash = FlashModule("FLASH", size=0x200000)
        self.flash.load(0, b"\x00" * 0x1F * 34)  # a few programmed blocks to erase

    def op(self, opcode, addr=0, wp=True):
        self.flash.write("ctrl", OPERATE, 4, addr | opcode << 24 | START | (WP if wp else 0))

    def sr(self):
        self.op(RDSR)
        return self.flash.read("ctrl", SR_REG, 4) & 0xFF

    def raw(self, addr, size=32):
        self.op(READ, addr & ~31)
        words = [self.flash.read("ctrl", FIFO_READ, 4) for _ in range(8)]
        return struct.pack("<8I", *words)[addr % 32:][:size]

    def set_sr(self, value):
        conf = self.flash.read("ctrl", CONF, 4) & ~(0xFFFF << 10)
        self.flash.write("ctrl", CONF, 4, conf | value << 10)
        self.op(WREN)
        self.op(WRSR)

    def fill_fifo(self, data32):
        for word in struct.unpack("<8I", data32):
            self.flash.write("ctrl", FIFO_WRITE, 4, word)

    def test_wren_and_wrdi_toggle_the_write_enable_latch(self):
        self.assertEqual(self.sr() & SR_WEL, 0)
        self.op(WREN)
        self.assertEqual(self.sr() & SR_WEL, SR_WEL)
        self.op(WRDI)
        self.assertEqual(self.sr() & SR_WEL, 0)

    def test_sector_erase_sets_the_aligned_4k_to_ff(self):
        self.flash.load(0x1012, b"\x11" * 34)  # first block boundary inside sector 1
        self.assertEqual(self.raw(0x1020, 16), b"\x11" * 16)
        self.op(WREN)
        self.op(SE, 0x1234)
        self.assertEqual(self.raw(0x1020, 16), b"\xff" * 16)
        self.assertEqual(self.raw(0x0400, 16), b"\x00" * 16)  # sector 0 is untouched

    def test_erase_needs_the_write_enable_latch_and_consumes_it(self):
        self.op(SE, 0)
        self.assertEqual(self.raw(0), b"\x00" * 32)
        self.op(WREN)
        self.op(SE, 0)
        self.assertEqual(self.raw(0), b"\xff" * 32)
        self.assertEqual(self.sr() & SR_WEL, 0)

    def test_erase_needs_wp_released_and_keeps_the_latch(self):
        self.op(WREN)
        self.op(SE, 0, wp=False)
        self.assertEqual(self.raw(0), b"\x00" * 32)
        self.assertEqual(self.sr() & SR_WEL, SR_WEL)

    def test_block_erases_and_chip_erase(self):
        self.flash.load(0x10010, b"\x22" * 34)  # block boundaries inside each 32 KB block
        self.flash.load(0x18018, b"\x22" * 34)
        self.op(WREN)
        self.op(BE1, 0x10123)
        self.assertEqual(self.raw(0x10020, 16), b"\xff" * 16)
        self.assertEqual(self.raw(0x18020, 16), b"\x22" * 16)  # the next 32 KB survives
        self.op(WREN)
        self.op(BE2, 0x18000)
        self.assertEqual(self.raw(0x18020, 16), b"\xff" * 16)
        self.op(WREN)
        self.op(CE)
        self.assertEqual(self.raw(0), b"\xff" * 32)

    def test_page_program_writes_the_fifo_into_an_erased_block(self):
        self.op(WREN)
        self.op(SE, 0)
        self.fill_fifo(bytes(range(32)))
        self.op(WREN)
        self.op(PP, 0x40)
        self.assertEqual(self.raw(0x40), bytes(range(32)))
        self.assertEqual(self.raw(0x20), b"\xff" * 32)

    def test_page_program_can_only_clear_bits(self):
        self.flash.load(0x1012, b"\xf0" * 34)  # raw bytes 0x1012..0x1033
        self.fill_fifo(b"\x3c" * 32)
        self.op(WREN)
        self.op(PP, 0x1020)
        self.assertEqual(self.raw(0x1020, 20), b"\x30" * 20)  # 0xf0 AND 0x3c
        self.assertEqual(self.raw(0x1034, 12), b"\x3c" * 12)  # erased bytes take the data

    def test_page_program_needs_the_latch_and_consumes_it(self):
        self.op(WREN)
        self.op(SE, 0)
        self.fill_fifo(b"\x55" * 32)
        self.op(PP, 0)
        self.assertEqual(self.raw(0), b"\xff" * 32)
        self.op(WREN)
        self.op(PP, 0)
        self.assertEqual(self.raw(0), b"\x55" * 32)
        self.assertEqual(self.sr() & SR_WEL, 0)

    def test_write_status_register_keeps_the_ro_bits(self):
        conf = self.flash.read("ctrl", CONF, 4) & ~(0xFFFF << 10)
        self.flash.write("ctrl", CONF, 4, conf | 0x7F << 10)
        self.op(WREN)
        self.op(WRSR)
        self.assertEqual(self.sr(), 0x7C)  # bits 0 (WIP) and 1 (WEL) are read-only

    def test_write_status_register_needs_no_explicit_write_enable(self):
        conf = self.flash.read("ctrl", CONF, 4) & ~(0xFFFF << 10)
        self.flash.write("ctrl", CONF, 4, conf | 0x7C << 10)
        self.op(WRSR)
        self.assertEqual(self.sr(), 0x7C)
        self.assertEqual(self.sr() & SR_WEL, 0)

    def test_write_status_register_2_needs_no_explicit_write_enable(self):
        conf = self.flash.read("ctrl", CONF, 4) & ~(0xFFFF << 10)
        self.flash.write("ctrl", CONF, 4, conf | 0x0200 << 10)
        self.op(WRSR2)
        self.op(RDSR2)
        self.assertEqual(self.flash.read("ctrl", SR_REG, 4) & 0xFF, 0x02)

    def test_page_program_still_needs_an_explicit_write_enable(self):
        self.op(WREN)
        self.op(SE, 0)
        self.fill_fifo(b"\x55" * 32)
        with self.assertLogs("emulator.flash", level="WARNING"):
            self.op(PP, 0)
        self.assertEqual(self.raw(0), b"\xff" * 32)

    def test_block_protection_suppresses_page_program_and_keeps_the_latch(self):
        self.set_sr(0x7C)
        self.op(WREN)
        self.fill_fifo(b"\x55" * 32)
        with self.assertLogs("emulator.flash", level="WARNING") as logs:
            self.op(PP, 0x40)
        self.assertIn("protect", logs.output[0])
        self.assertEqual(self.raw(0x40), b"\x00" * 32)
        self.assertEqual(self.sr() & SR_WEL, SR_WEL)
        self.set_sr(0)  # WREN + WRSR(0) clears the protection
        self.op(WREN)
        self.op(PP, 0x40)
        self.assertEqual(self.raw(0x40), b"\x00" * 32)  # 0x55 AND 0x00: nothing erased here
        self.assertEqual(self.sr(), 0)

    def test_block_protection_also_blocks_erase(self):
        self.set_sr(0x7C)
        self.op(WREN)
        with self.assertLogs("emulator.flash", level="WARNING"):
            self.op(SE, 0)
        self.assertEqual(self.raw(0), b"\x00" * 32)

    def test_wrsr2_stores_both_status_bytes(self):
        conf = self.flash.read("ctrl", CONF, 4) & ~(0xFFFF << 10)
        self.flash.write("ctrl", CONF, 4, conf | 0x0200 << 10)
        self.op(WREN)
        self.op(WRSR2)
        self.op(RDSR2)
        self.assertEqual(self.flash.read("ctrl", SR_REG, 4) & 0xFF, 0x02)

    def test_programming_makes_an_xip_block_readable(self):
        self.op(WREN)
        self.op(SE, 0x11000)
        good = wrap(bytes(range(32)))
        self.fill_fifo(good[:32])
        self.op(WREN)
        self.op(PP, 0x11000)
        self.fill_fifo(good[32:] + b"\xff" * 30)
        self.op(WREN)
        self.op(PP, 0x11020)
        self.assertEqual(self.flash.read("xip", 0x10000, 4), 0x03020100)

    def test_erase_makes_a_good_xip_block_bad_again(self):
        self.flash.load(0x11000, image(block(7)))
        self.assertEqual(self.flash.read("xip", 0x10000, 1), 7)
        self.op(WREN)
        self.op(SE, 0x11000)
        with self.assertLogs("emulator.flash", level="ERROR"):
            with self.assertRaises(Fault):
                self.flash.read("xip", 0x10000, 1)

    def test_changed_flash_invalidates_the_cached_pages(self):
        calls = []
        self.flash.invalidate = lambda *args: calls.append(args)
        self.op(WREN)
        self.op(SE, 0x11000)
        self.assertEqual(len(calls), 1)
        name, start, size = calls[0]
        self.assertEqual(name, "xip")
        self.assertLessEqual(start, 0x10000)
        self.assertGreaterEqual(start + size, 0x10000 + 0x1000 * 32 // 34)

    def debug(self):
        return self.assertLogs("emulator.flash", level="DEBUG")

    def test_erases_are_decoded(self):
        with self.debug() as logs:
            self.op(WREN)
            self.op(SE, 0x11234)
            self.op(WREN)
            self.op(BE1, 0x10123)
            self.op(WREN)
            self.op(BE2, 0x18000)
            self.op(WREN)
            self.op(CE)
        text = "\n".join(logs.output)
        self.assertIn("erase 4 KiB sector 0x00011000-0x00011FFF (XIP blocks 2048-2168)", text)
        self.assertIn("erase 32 KiB block 0x00010000-0x00017FFF", text)
        self.assertIn("erase 64 KiB block 0x00010000-0x0001FFFF", text)
        self.assertIn("erase whole chip", text)

    def test_page_program_is_decoded_with_where_it_lands(self):
        self.fill_fifo(bytes(range(32)))
        self.op(WREN)
        with self.debug() as logs:
            self.op(PP, 0x11000)
        self.assertIn("program 32 bytes at 0x00011000 (XIP block 2048 byte 0)", logs.output[0])
        self.assertIn("00 01 02 03 04 05 06 07", logs.output[0])
        self.fill_fifo(bytes(32))
        self.op(WREN)
        with self.debug() as logs:
            self.op(PP, 0x11020)
        self.assertIn("block 2048 crc byte 0", logs.output[0])

    def test_status_register_writes_are_decoded(self):
        with self.debug() as logs:
            self.set_sr(0x7C)
            self.set_sr(0)
        text = "\n".join(logs.output)
        self.assertIn("write status register 1 = 0x7C (BP2 BP1 BP0 TB 4KBL)", text)
        self.assertIn("write status register 1 = 0x00 (no protection)", text)

    def test_write_enable_and_disable_are_decoded(self):
        with self.debug() as logs:
            self.op(WREN)
            self.op(WRDI)
        text = "\n".join(logs.output)
        self.assertIn("write enable latch set", text)
        self.assertIn("write enable latch cleared", text)

    def test_reads_are_not_logged(self):
        self.raw(0)
        with self.assertNoLogs("emulator.flash", level="DEBUG"):
            self.raw(0)

    def test_reset_clears_the_status_register_and_latch(self):
        self.set_sr(0x7C)
        self.op(WREN)
        self.flash.reset()
        self.assertEqual(self.sr(), 0)


if __name__ == "__main__":
    unittest.main()
