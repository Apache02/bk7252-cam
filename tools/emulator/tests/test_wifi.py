import unittest

from modules.wifi import (
    Agc, AgcRam, La, MacCore, MacPl, MacPta, MdmCfg, MdmStat, NxDma, NxIntc, Rc,
)

# block, size in bytes: sizeof(hw_*_t) from src/platform/soc/include/rwnx/*.h
# (AgcRam: the 512-word microcode image the radio code writes at 0x0100A000)
BLOCKS = [
    (AgcRam, 512 * 4),
    (MacCore, 1376 * 4), (MacPl, 351 * 4), (MacPta, 10 * 4), (MdmCfg, 38 * 4),
    (MdmStat, 27 * 4), (Agc, 44 * 4), (Rc, 480), (NxIntc, 17 * 4), (NxDma, 49 * 4), (La, 16 * 4),
]


def make(cls, **params):
    return cls("BLOCK", **params)


class RegisterBankTest(unittest.TestCase):
    def test_every_block_is_one_range_of_its_header_size(self):
        for cls, size in BLOCKS:
            with self.subTest(block=cls.__name__):
                self.assertEqual(make(cls).ranges(), [("regs", size)])

    def test_words_read_back_and_start_at_zero(self):
        for cls, size in BLOCKS:
            with self.subTest(block=cls.__name__):
                block = make(cls)
                self.assertEqual(block.read("regs", 0x10, 4), 0)
                block.write("regs", 0x10, 4, 0xDEADBEEF)
                self.assertEqual(block.read("regs", 0x10, 4), 0xDEADBEEF)
                last = size - 4
                block.write("regs", last, 4, 0x12345678)
                self.assertEqual(block.read("regs", last, 4), 0x12345678)

    def test_bytes_and_halfwords_compose_into_words_little_endian(self):
        block = make(MdmCfg)
        block.write("regs", 0x00, 1, 0x11)
        block.write("regs", 0x01, 1, 0x22)
        block.write("regs", 0x02, 2, 0x4433)
        self.assertEqual(block.read("regs", 0x00, 4), 0x44332211)
        self.assertEqual(block.read("regs", 0x02, 2), 0x4433)
        self.assertEqual(block.read("regs", 0x01, 1), 0x22)

    def test_a_write_changes_only_its_own_bytes(self):
        block = make(Rc)
        block.write("regs", 0x80, 4, 0xFFFFFFFF)
        block.write("regs", 0x81, 1, 0x00)
        self.assertEqual(block.read("regs", 0x80, 4), 0xFFFF00FF)

    def test_reset_clears_the_registers(self):
        block = make(Agc)
        block.write("regs", 0x20, 4, 5)
        block.reset()
        self.assertEqual(block.read("regs", 0x20, 4), 0)


class DebugLogTest(unittest.TestCase):
    def test_reads_and_writes_are_logged_with_block_offset_size_and_value(self):
        block = MacPl("MAC_PL")
        with self.assertLogs("emulator.wifi", level="DEBUG") as logs:
            block.write("regs", 0x10, 4, 0xFF)
            block.read("regs", 0x10, 4)
            block.write("regs", 0x11, 1, 0xAB)
        self.assertEqual(logs.output[0], "DEBUG:emulator.wifi.mac_pl:MAC_PL write +0x0010 (4 bytes) = 0x000000FF")
        self.assertEqual(logs.output[1], "DEBUG:emulator.wifi.mac_pl:MAC_PL read +0x0010 (4 bytes) = 0x000000FF")
        self.assertEqual(logs.output[2], "DEBUG:emulator.wifi.mac_pl:MAC_PL write +0x0011 (1 bytes) = 0xAB")

    def test_each_block_has_its_own_logger_under_wifi(self):
        names = set()
        for cls, _ in BLOCKS:
            with self.assertLogs("emulator.wifi", level="DEBUG") as logs:
                make(cls).read("regs", 0, 4)
            names.add(logs.records[0].name)
        self.assertEqual(len(names), len(BLOCKS))
        self.assertTrue(all(n.startswith("emulator.wifi.") for n in names))

    def test_nothing_is_logged_without_debug(self):
        block = MacPl("MAC_PL")
        with self.assertNoLogs("emulator.wifi", level="INFO"):
            block.write("regs", 0, 4, 1)
            block.read("regs", 0, 4)


if __name__ == "__main__":
    unittest.main()
