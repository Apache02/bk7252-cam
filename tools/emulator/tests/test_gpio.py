import unittest

from modules.gpio import GpioModule

IN_EN, OUT_DIS, PULL_MODE, PULL_EN, FUNC_EN = 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6
OUTPUT, INPUT, MONITOR = 1 << 1, 1 << 0, 1 << 7
INTEN, INTLV0, INTLV1, INTSTA = 0x84, 0x88, 0x8C, 0x90
INTEN2, INTLV3, INTSTA2 = 0xE0, 0xE4, 0xE8
LOW, HIGH, RISING, FALLING = 0, 1, 2, 3


class Line:
    def __init__(self):
        self.level = False

    def set(self, level):
        self.level = level


def make(**params):
    line = Line()
    gpio = GpioModule("GPIO", irq=line, pin_reset=0x28, **params)
    return gpio, line


def pin_offset(pin):
    return pin * 4 if pin < 32 else 0xC0 + (pin - 32) * 4


def cfg(gpio, pin, value=None):
    if value is None:
        return gpio.read("regs", pin_offset(pin), 4)
    gpio.write("regs", pin_offset(pin), 4, value)


def reg(gpio, offset, value=None):
    if value is None:
        return gpio.read("regs", offset, 4)
    gpio.write("regs", offset, 4, value)


def arm_interrupt(gpio, pin, mode):
    """Configure `pin` as an input and enable its interrupt with `mode`."""
    cfg(gpio, pin, IN_EN | OUT_DIS | PULL_EN | (PULL_MODE if mode in (LOW, FALLING) else 0))
    if pin < 16:
        reg(gpio, INTLV0, reg(gpio, INTLV0) | mode << (pin * 2))
    elif pin < 32:
        reg(gpio, INTLV1, reg(gpio, INTLV1) | mode << ((pin - 16) * 2))
    else:
        reg(gpio, INTLV3, reg(gpio, INTLV3) | mode << ((pin - 32) * 2))
    if pin < 32:
        reg(gpio, INTEN, reg(gpio, INTEN) | 1 << pin)
    else:
        reg(gpio, INTEN2, reg(gpio, INTEN2) | 1 << (pin - 32))


class PadTest(unittest.TestCase):
    def test_pin_reset_value_and_pull_down_default(self):
        gpio, _ = make()
        self.assertEqual(cfg(gpio, 3), 0x28)
        self.assertEqual(cfg(gpio, 39), 0x28)

    def test_reset_words_override_individual_pins(self):
        gpio, _ = make(reset_words={"0x050": 0x68})
        self.assertEqual(cfg(gpio, 20), 0x68)
        self.assertEqual(cfg(gpio, 21), 0x28)

    def test_output_pin_drives_the_pad_and_reads_back(self):
        gpio, _ = make()
        cfg(gpio, 5, OUTPUT)  # output driver on, high
        self.assertEqual(cfg(gpio, 5), OUTPUT | INPUT)
        cfg(gpio, 5, 0)
        self.assertEqual(cfg(gpio, 5), 0)

    def test_input_pin_follows_the_external_level(self):
        gpio, _ = make()
        cfg(gpio, 5, IN_EN | OUT_DIS)
        gpio.set_input(5, True)
        self.assertTrue(cfg(gpio, 5) & INPUT)
        gpio.set_input(5, False)
        self.assertFalse(cfg(gpio, 5) & INPUT)

    def test_released_input_uses_the_pull(self):
        gpio, _ = make()
        cfg(gpio, 5, IN_EN | OUT_DIS | PULL_EN | PULL_MODE)
        self.assertTrue(cfg(gpio, 5) & INPUT)
        cfg(gpio, 5, IN_EN | OUT_DIS | PULL_EN)
        self.assertFalse(cfg(gpio, 5) & INPUT)
        cfg(gpio, 5, IN_EN | OUT_DIS)
        self.assertFalse(cfg(gpio, 5) & INPUT)  # floating reads 0

    def test_external_level_overrides_the_pull(self):
        gpio, _ = make()
        cfg(gpio, 5, IN_EN | OUT_DIS | PULL_EN | PULL_MODE)
        gpio.set_input(5, False)
        self.assertFalse(cfg(gpio, 5) & INPUT)
        gpio.set_input(5, None)
        self.assertTrue(cfg(gpio, 5) & INPUT)

    def test_driven_pad_ignores_the_external_level(self):
        gpio, _ = make()
        cfg(gpio, 5, 0)
        gpio.set_input(5, True)
        self.assertFalse(cfg(gpio, 5) & INPUT)

    def test_input_and_monitor_bits_are_read_only(self):
        gpio, _ = make()
        cfg(gpio, 5, INPUT | MONITOR | OUT_DIS)
        self.assertEqual(cfg(gpio, 5), OUT_DIS)

    def test_bank1_pins_use_their_own_registers(self):
        gpio, _ = make()
        cfg(gpio, 33, OUTPUT)
        self.assertEqual(reg(gpio, 0xC4), OUTPUT | INPUT)
        self.assertEqual(cfg(gpio, 1), 0x28)

    def test_watchers_see_pad_changes_once(self):
        gpio, _ = make()
        seen = []
        gpio.watch(lambda pin, level: seen.append((pin, level)))
        cfg(gpio, 4, OUTPUT)
        cfg(gpio, 4, OUTPUT)  # same level again: no event
        cfg(gpio, 4, 0)
        self.assertEqual(seen, [(4, True), (4, False)])

    def test_pad_level_reports_what_the_pin_sits_at(self):
        gpio, _ = make()
        cfg(gpio, 4, OUTPUT)
        self.assertTrue(gpio.pad_level(4))

    def test_pin_out_of_range_rejected(self):
        gpio, _ = make()
        with self.assertRaisesRegex(ValueError, "40"):
            gpio.set_input(40, True)

    def test_other_registers_read_back_and_detect_is_read_only(self):
        gpio, _ = make(reset_words={"0x09c": 2})
        reg(gpio, 0x80, 0x5500)
        self.assertEqual(reg(gpio, 0x80), 0x5500)
        reg(gpio, 0x9C, 0)
        self.assertEqual(reg(gpio, 0x9C), 2)


class InterruptTest(unittest.TestCase):
    def test_rising_edge_latches_and_raises_the_line(self):
        gpio, line = make()
        arm_interrupt(gpio, 5, RISING)
        gpio.set_input(5, True)
        self.assertEqual(reg(gpio, INTSTA), 1 << 5)
        self.assertTrue(line.level)
        reg(gpio, INTSTA, 1 << 5)
        self.assertEqual(reg(gpio, INTSTA), 0)
        self.assertFalse(line.level)

    def test_rising_mode_ignores_the_falling_edge(self):
        gpio, line = make()
        arm_interrupt(gpio, 5, RISING)
        gpio.set_input(5, True)
        reg(gpio, INTSTA, 1 << 5)
        gpio.set_input(5, False)
        self.assertEqual(reg(gpio, INTSTA), 0)

    def test_falling_edge(self):
        gpio, _ = make()
        arm_interrupt(gpio, 5, FALLING)
        gpio.set_input(5, True)
        self.assertEqual(reg(gpio, INTSTA), 0)
        gpio.set_input(5, False)
        self.assertEqual(reg(gpio, INTSTA), 1 << 5)

    def test_level_low_stays_pending_until_the_level_goes_away(self):
        gpio, line = make()
        arm_interrupt(gpio, 20, LOW)  # pull-up input, idle high
        self.assertEqual(reg(gpio, INTSTA), 0)
        gpio.set_input(20, False)
        self.assertEqual(reg(gpio, INTSTA), 1 << 20)
        reg(gpio, INTSTA, 1 << 20)
        self.assertEqual(reg(gpio, INTSTA), 1 << 20)  # still low: latches again
        gpio.set_input(20, True)
        reg(gpio, INTSTA, 1 << 20)
        self.assertEqual(reg(gpio, INTSTA), 0)
        self.assertFalse(line.level)

    def test_level_high(self):
        gpio, _ = make()
        arm_interrupt(gpio, 3, HIGH)
        gpio.set_input(3, True)
        self.assertEqual(reg(gpio, INTSTA), 1 << 3)

    def test_disabled_pin_reports_nothing(self):
        gpio, line = make()
        cfg(gpio, 5, IN_EN | OUT_DIS)
        gpio.set_input(5, False)
        gpio.set_input(5, True)
        self.assertEqual(reg(gpio, INTSTA), 0)
        self.assertFalse(line.level)

    def test_pins_32_to_39_use_the_second_set_of_registers(self):
        gpio, line = make()
        arm_interrupt(gpio, 35, RISING)
        gpio.set_input(35, True)
        self.assertEqual(reg(gpio, INTSTA2), 1 << 3)
        self.assertEqual(reg(gpio, INTSTA), 0)
        self.assertTrue(line.level)
        reg(gpio, INTSTA2, 1 << 3)
        self.assertFalse(line.level)

    def test_reset_clears_state_but_keeps_the_outside_world(self):
        gpio, line = make(reset_words={"0x050": 0x68})
        arm_interrupt(gpio, 5, RISING)
        gpio.set_input(5, True)
        cfg(gpio, 7, OUTPUT)
        gpio.reset()
        self.assertFalse(line.level)
        self.assertEqual(reg(gpio, INTSTA), 0)
        self.assertEqual(reg(gpio, INTEN), 0)
        self.assertEqual(cfg(gpio, 7), 0x28)
        self.assertEqual(cfg(gpio, 20), 0x68)
        cfg(gpio, 5, IN_EN | OUT_DIS)
        self.assertTrue(cfg(gpio, 5) & INPUT)  # the external level survived


if __name__ == "__main__":
    unittest.main()
