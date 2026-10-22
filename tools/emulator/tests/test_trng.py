import unittest

from modules.trng import TrngModule

CTRL, DATA = 0x00, 0x04


def reg(trng, offset, value=None):
    if value is None:
        return trng.read("regs", offset, 4)
    trng.write("regs", offset, 4, value)


class TrngTest(unittest.TestCase):
    def test_enabled_from_the_start(self):
        self.assertEqual(reg(TrngModule("TRNG"), CTRL), 1)

    def test_data_counts_reads_from_zero(self):
        trng = TrngModule("TRNG")
        self.assertEqual([reg(trng, DATA) for _ in range(5)], [0, 1, 2, 3, 4])

    def test_control_reads_back_what_was_written(self):
        trng = TrngModule("TRNG")
        reg(trng, CTRL, 0)
        self.assertEqual(reg(trng, CTRL), 0)

    def test_disabled_data_is_a_latch_and_does_not_count(self):
        trng = TrngModule("TRNG")
        reg(trng, CTRL, 0)
        reg(trng, DATA, 0x1234)
        self.assertEqual([reg(trng, DATA), reg(trng, DATA)], [0x1234, 0x1234])
        reg(trng, CTRL, 1)
        self.assertEqual(reg(trng, DATA), 0)

    def test_writes_to_data_are_ignored_while_enabled(self):
        trng = TrngModule("TRNG")
        reg(trng, DATA, 0x1234)
        self.assertEqual(reg(trng, DATA), 0)

    def test_reset_restarts_the_count_and_enables(self):
        trng = TrngModule("TRNG")
        reg(trng, DATA)
        reg(trng, DATA)
        reg(trng, CTRL, 0)
        trng.reset()
        self.assertEqual(reg(trng, CTRL), 1)
        self.assertEqual(reg(trng, DATA), 0)

    def test_range_is_two_words(self):
        self.assertEqual(TrngModule("TRNG").ranges(), [("regs", 8)])


if __name__ == "__main__":
    unittest.main()
