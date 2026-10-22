import unittest

from modules.sctrl import SctrlModule

CHIP_ID, DEVICE_ID, CONTROL, CHARGE_STATUS = 0x00, 0x04, 0x08, 0x84


def make(**params):
    return SctrlModule("SCTRL", **params)


def reg(sctrl, offset, value=None):
    if value is None:
        return sctrl.read("regs", offset, 4)
    sctrl.write("regs", offset, 4, value)


class SctrlTest(unittest.TestCase):
    def test_ids_come_from_the_profile(self):
        sctrl = make(chip_id=0x7221A, device_id=0x18221020)
        self.assertEqual(reg(sctrl, CHIP_ID), 0x7221A)
        self.assertEqual(reg(sctrl, DEVICE_ID), 0x18221020)

    def test_ids_default_to_zero(self):
        sctrl = make()
        self.assertEqual((reg(sctrl, CHIP_ID), reg(sctrl, DEVICE_ID)), (0, 0))

    def test_ids_are_read_only(self):
        sctrl = make(chip_id=0x7221A, device_id=0x18221020)
        reg(sctrl, CHIP_ID, 0)
        reg(sctrl, DEVICE_ID, 0)
        self.assertEqual(reg(sctrl, CHIP_ID), 0x7221A)
        self.assertEqual(reg(sctrl, DEVICE_ID), 0x18221020)

    def test_registers_read_back_and_unknown_ones_read_zero(self):
        sctrl = make()
        self.assertEqual(reg(sctrl, 0x58), 0)
        reg(sctrl, 0x58, 0xDEADBEEF)
        self.assertEqual(reg(sctrl, 0x58), 0xDEADBEEF)

    def test_reset_words_set_register_values(self):
        sctrl = make(reset_words={"0x008": 0x00020000, "0x12c": 0x147})
        self.assertEqual(reg(sctrl, CONTROL), 0x00020000)
        self.assertEqual(reg(sctrl, 0x12C), 0x147)

    def test_reset_restores_register_values_and_keeps_ids(self):
        sctrl = make(chip_id=0x7221A, reset_words={"0x008": 0x00020000})
        reg(sctrl, CONTROL, 0xFFFF)
        reg(sctrl, 0x150, 0x1234)  # software retention word
        sctrl.reset()
        self.assertEqual(reg(sctrl, CONTROL), 0x00020000)
        self.assertEqual(reg(sctrl, 0x150), 0)
        self.assertEqual(reg(sctrl, CHIP_ID), 0x7221A)

    def test_warm_reset_words_apply_only_after_a_chip_reset(self):
        sctrl = make(
            reset_words={"0x084": 0xFFE08410},
            warm_reset_words={"0x084": 0xFFE08010},
        )
        self.assertEqual(reg(sctrl, CHARGE_STATUS), 0xFFE08410)
        sctrl.reset()
        self.assertEqual(reg(sctrl, CHARGE_STATUS), 0xFFE08010)

    DIVIDER_BIAS = [{"offset": 0x08, "shift": 4, "width": 4, "add": 1}]

    def test_a_biased_field_reads_one_more_than_was_written(self):
        sctrl = make(read_adjust=self.DIVIDER_BIAS)
        reg(sctrl, CONTROL, 0x30)  # divider 3
        self.assertEqual(reg(sctrl, CONTROL), 0x40)

    def test_the_bias_leaves_the_other_bits_alone(self):
        sctrl = make(read_adjust=self.DIVIDER_BIAS)
        reg(sctrl, CONTROL, 0x00F20132)  # divider 3, other fields set
        self.assertEqual(reg(sctrl, CONTROL), 0x00F20142)

    def test_the_bias_stops_at_the_top_of_the_field(self):
        sctrl = make(read_adjust=self.DIVIDER_BIAS)
        reg(sctrl, CONTROL, 0xF0)
        self.assertEqual(reg(sctrl, CONTROL), 0xF0)  # no carry into bit 8

    def test_the_bias_applies_to_the_reset_value_too(self):
        sctrl = make(read_adjust=self.DIVIDER_BIAS, reset_words={"0x008": 0x00020000})
        self.assertEqual(reg(sctrl, CONTROL), 0x00020010)

    def test_the_bias_touches_only_its_register(self):
        sctrl = make(read_adjust=self.DIVIDER_BIAS)
        reg(sctrl, 0x58, 0x30)
        self.assertEqual(reg(sctrl, 0x58), 0x30)

    def test_without_the_parameter_reads_are_exact(self):
        sctrl = make()
        reg(sctrl, CONTROL, 0x30)
        self.assertEqual(reg(sctrl, CONTROL), 0x30)

    def test_signals_watch_register_fields(self):
        sctrl = make(signals={"mac_powered": {"offset": 0x10C, "shift": 16, "width": 16, "equals": 1}})
        signal = sctrl.signal("mac_powered")
        self.assertFalse(signal.active())
        reg(sctrl, 0x10C, 0x0001 << 16)
        self.assertTrue(signal.active())
        reg(sctrl, 0x10C, 0xD802 << 16)
        self.assertFalse(signal.active())

    def test_unknown_signal_rejected(self):
        with self.assertRaisesRegex(KeyError, "nope"):
            make().signal("nope")

    def test_range_covers_the_block(self):
        sizes = dict(make().ranges())
        self.assertGreaterEqual(sizes["regs"], 0x154)


if __name__ == "__main__":
    unittest.main()
