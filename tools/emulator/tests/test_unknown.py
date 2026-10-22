import unittest

from modules.unknown import UnknownModule


def make(instruction=None, **params):
    return UnknownModule("MYSTERY", cpu_instruction=instruction, **params)


class UnknownModuleTest(unittest.TestCase):
    def test_it_is_one_range_of_the_given_size(self):
        self.assertEqual(make().ranges(), [("regs", 0x100)])
        self.assertEqual(make(size=0x40).ranges(), [("regs", 0x40)])

    def test_it_is_plain_memory_that_starts_at_zero(self):
        block = make()
        self.assertEqual(block.read("regs", 0x10, 4), 0)
        block.write("regs", 0x10, 4, 0xDEADBEEF)
        self.assertEqual(block.read("regs", 0x10, 4), 0xDEADBEEF)
        block.write("regs", 0x11, 1, 0x00)
        self.assertEqual(block.read("regs", 0x10, 4), 0xDEAD00EF)

    def test_reset_words_give_the_start_values_and_a_reset_restores_them(self):
        block = make(reset_words={"0x04": 0x6236, "0x08": 7})
        self.assertEqual((block.read("regs", 4, 4), block.read("regs", 8, 4)), (0x6236, 7))
        block.write("regs", 4, 4, 1)
        block.reset()
        self.assertEqual(block.read("regs", 4, 4), 0x6236)

    def test_every_read_is_an_info_line_naming_the_block_offset_value_and_instruction(self):
        block = make(instruction=lambda: (0x00012344, 0xE5910000, 4), reset_words={"0x10": 0x6236})
        with self.assertLogs("emulator.unknown", level="INFO") as logs:
            block.read("regs", 0x10, 4)
            block.read("regs", 0x10, 4)
        self.assertEqual(len(logs.output), 2)
        line = logs.output[0]
        for text in ("INFO", "MYSTERY", "read", "+0x0010", "0x00006236", "pc 0x00012344", "E5910000"):
            self.assertIn(text, line)

    def test_every_write_is_an_info_line_with_the_value_written(self):
        block = make(instruction=lambda: (0x00012344, 0xE5801000, 4))
        with self.assertLogs("emulator.unknown", level="INFO") as logs:
            block.write("regs", 0x20, 4, 0xCAFE)
        for text in ("write", "+0x0020", "0x0000CAFE", "pc 0x00012344", "E5801000"):
            self.assertIn(text, logs.output[0])

    def test_a_thumb_instruction_shows_four_hex_digits(self):
        block = make(instruction=lambda: (0x00012346, 0x6008, 2))
        with self.assertLogs("emulator.unknown", level="INFO") as logs:
            block.write("regs", 0, 4, 1)
        self.assertIn("insn 6008", logs.output[0])

    def test_without_a_cpu_the_line_has_no_instruction(self):
        block = make()
        with self.assertLogs("emulator.unknown", level="INFO") as logs:
            block.read("regs", 0, 4)
        self.assertNotIn("pc", logs.output[0])


if __name__ == "__main__":
    unittest.main()
