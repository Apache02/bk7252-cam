import unittest

from modules.wifi import MacPl

MAC_CNTRL_2 = 0x50


def reg(block):
    return block.read("regs", MAC_CNTRL_2, 4)


class SoftResetTest(unittest.TestCase):
    def test_the_bit_reads_set_until_the_next_tick_then_clears_itself(self):
        block = MacPl("MAC_PL")
        block.write("regs", MAC_CNTRL_2, 4, 1)
        self.assertEqual(reg(block), 1)
        self.assertEqual(reg(block), 1)
        block.tick(10_000)
        self.assertEqual(reg(block), 0)

    def test_it_stays_clear_afterwards(self):
        block = MacPl("MAC_PL")
        block.write("regs", MAC_CNTRL_2, 4, 1)
        block.tick(10_000)
        block.tick(20_000)
        self.assertEqual(reg(block), 0)

    def test_writing_zero_never_sets_it(self):
        block = MacPl("MAC_PL")
        block.write("regs", MAC_CNTRL_2, 4, 0)
        self.assertEqual(reg(block), 0)

    def test_a_second_reset_request_works_again(self):
        block = MacPl("MAC_PL")
        for _ in range(2):
            block.write("regs", MAC_CNTRL_2, 4, 1)
            self.assertEqual(reg(block), 1)
            block.tick(0)
            self.assertEqual(reg(block), 0)

    def test_only_the_reset_bit_clears(self):
        block = MacPl("MAC_PL")
        block.write("regs", MAC_CNTRL_2, 4, 0x81)
        block.tick(0)
        self.assertEqual(reg(block), 0x80)

    def test_a_byte_write_of_the_low_byte_triggers_it_too(self):
        block = MacPl("MAC_PL")
        block.write("regs", MAC_CNTRL_2, 1, 1)
        self.assertEqual(reg(block), 1)
        block.tick(0)
        self.assertEqual(reg(block), 0)

    def test_other_registers_are_untouched_by_ticks(self):
        block = MacPl("MAC_PL")
        block.write("regs", 0x48, 4, 1)
        block.tick(0)
        self.assertEqual(block.read("regs", 0x48, 4), 1)

    def test_chip_reset_forgets_a_pending_reset(self):
        block = MacPl("MAC_PL")
        block.write("regs", MAC_CNTRL_2, 4, 1)
        block.reset()
        block.tick(0)
        self.assertEqual(reg(block), 0)

    def test_completion_shows_in_the_debug_log(self):
        block = MacPl("MAC_PL")
        block.write("regs", MAC_CNTRL_2, 4, 1)
        with self.assertLogs("emulator.wifi", level="DEBUG") as logs:
            block.tick(0)
        self.assertIn("MAC_PL soft reset done", logs.output[0])


if __name__ == "__main__":
    unittest.main()
