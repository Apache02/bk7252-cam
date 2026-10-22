import unittest

from modules.saradc import SaradcModule

CONFIG, DATA, CTRL_CFG, SAT_CFG, DAT_AFTER_STA = 0x00, 0x04, 0x08, 0x0C, 0x10
CHNL_EN, INT_CLR = 1 << 2, 1 << 8
BUSY, FIFO_EMPTY, FIFO_FULL = 1 << 29, 1 << 30, 1 << 31
SAMPLE_NS = 100_000
DEPTH = 32


class Line:
    def __init__(self):
        self.level = False

    def set(self, level):
        self.level = level


def make(**params):
    line = Line()
    return SaradcModule("SAR_ADC", irq=line, **params), line


def reg(adc, offset, value=None):
    if value is None:
        return adc.read("regs", offset, 4)
    adc.write("regs", offset, 4, value)


def started(**params):
    """A converter whose channel was enabled at time 0."""
    adc, line = make(**params)
    reg(adc, CONFIG, CHNL_EN)
    return adc, line


class RegisterTest(unittest.TestCase):
    def test_it_starts_with_an_empty_fifo(self):
        adc, line = make()
        self.assertEqual(reg(adc, CONFIG), FIFO_EMPTY)
        self.assertFalse(line.level)
        self.assertIsNone(adc.next_event_ns())

    def test_the_reset_values_are_those_of_the_chip(self):
        adc, _ = make()
        self.assertEqual(reg(adc, CTRL_CFG), 0xE0)
        self.assertEqual(reg(adc, SAT_CFG), 0)
        self.assertEqual(reg(adc, DAT_AFTER_STA), 0xFFFF)
        self.assertEqual(reg(adc, DATA), 0)

    def test_config_keeps_what_was_written_but_not_the_status_bits_or_the_clear_command(self):
        adc, _ = make()
        reg(adc, CONFIG, 0xFFFFFFFF & ~CHNL_EN)
        # bits 29-31 are status, bit 8 is the command; the FIFO is empty
        self.assertEqual(reg(adc, CONFIG), 0x1FFFFEFB | FIFO_EMPTY)

    def test_the_interrupt_clear_bit_reads_back_zero_so_the_drivers_loop_ends(self):
        adc, _ = make()
        reg(adc, CONFIG, 0x00202100)
        self.assertEqual(reg(adc, CONFIG) & INT_CLR, 0)
        self.assertEqual(reg(adc, CONFIG) & 0x00202000, 0x00202000)

    def test_the_other_registers_read_back(self):
        adc, _ = make()
        reg(adc, CTRL_CFG, 0x123)
        reg(adc, SAT_CFG, 0x5)
        self.assertEqual((reg(adc, CTRL_CFG), reg(adc, SAT_CFG)), (0x123, 0x5))


class ConversionTest(unittest.TestCase):
    def test_a_sample_arrives_one_period_after_the_channel_is_enabled(self):
        adc, _ = started()
        self.assertEqual(adc.next_event_ns(), SAMPLE_NS)
        adc.tick(SAMPLE_NS - 1)
        self.assertTrue(reg(adc, CONFIG) & FIFO_EMPTY)
        adc.tick(SAMPLE_NS)
        self.assertFalse(reg(adc, CONFIG) & FIFO_EMPTY)

    def test_the_period_starts_at_the_moment_of_the_enable(self):
        adc, _ = make(clock=None)
        adc.tick(5_000_000)
        reg(adc, CONFIG, CHNL_EN)
        self.assertEqual(adc.next_event_ns(), 5_000_000 + SAMPLE_NS)

    def test_reading_data_pops_a_sample_with_the_configured_value(self):
        adc, _ = started(sample=777)
        adc.tick(SAMPLE_NS)
        self.assertEqual(reg(adc, DATA), 777)
        self.assertTrue(reg(adc, CONFIG) & FIFO_EMPTY)

    def test_reading_dat_after_sta_pops_a_sample_too(self):
        adc, _ = started()
        adc.tick(SAMPLE_NS)
        self.assertEqual(reg(adc, DAT_AFTER_STA), 330)
        self.assertTrue(reg(adc, CONFIG) & FIFO_EMPTY)

    def test_samples_keep_coming_until_the_fifo_is_full(self):
        adc, _ = started()
        adc.tick(100 * SAMPLE_NS)
        status = reg(adc, CONFIG)
        self.assertTrue(status & FIFO_FULL)
        self.assertFalse(status & FIFO_EMPTY)
        popped = 0
        while not reg(adc, CONFIG) & FIFO_EMPTY:
            reg(adc, DAT_AFTER_STA)
            popped += 1
        self.assertEqual(popped, DEPTH)

    def test_the_isr_drain_loop_ends_when_the_fifo_is_empty(self):
        adc, _ = started()
        adc.tick(3 * SAMPLE_NS)
        values = []
        while not reg(adc, CONFIG) & FIFO_EMPTY:
            values.append(reg(adc, DAT_AFTER_STA) & 0xFFFF)
        self.assertEqual(values, [330, 330, 330])

    def test_disabling_the_channel_stops_sampling_and_flushes_the_fifo(self):
        adc, _ = started()
        adc.tick(3 * SAMPLE_NS)
        reg(adc, CONFIG, 0)
        self.assertTrue(reg(adc, CONFIG) & FIFO_EMPTY)
        self.assertIsNone(adc.next_event_ns())
        adc.tick(10 * SAMPLE_NS)
        self.assertTrue(reg(adc, CONFIG) & FIFO_EMPTY)

    def test_a_reset_empties_everything(self):
        adc, line = started()
        adc.tick(3 * SAMPLE_NS)
        adc.reset()
        self.assertEqual(reg(adc, CONFIG), FIFO_EMPTY)
        self.assertIsNone(adc.next_event_ns())
        self.assertFalse(line.level)


class InterruptTest(unittest.TestCase):
    def test_a_sample_raises_the_line_until_the_interrupt_is_cleared(self):
        adc, line = started()
        adc.tick(SAMPLE_NS)
        self.assertTrue(line.level)
        reg(adc, DATA)
        self.assertTrue(line.level)
        reg(adc, CONFIG, CHNL_EN | INT_CLR)
        self.assertFalse(line.level)

    def test_the_next_sample_raises_it_again(self):
        adc, line = started()
        adc.tick(SAMPLE_NS)
        reg(adc, CONFIG, CHNL_EN | INT_CLR)
        adc.tick(2 * SAMPLE_NS)
        self.assertTrue(line.level)

    def test_almost_cfg_is_the_number_of_samples_that_raises_it(self):
        adc, line = make()
        reg(adc, CTRL_CFG, 4)
        reg(adc, CONFIG, CHNL_EN)
        adc.tick(3 * SAMPLE_NS)
        self.assertFalse(line.level)
        adc.tick(4 * SAMPLE_NS)
        self.assertTrue(line.level)

    def test_a_driver_that_closes_the_channel_after_a_few_samples_leaves_no_storm(self):
        adc, line = started()
        adc.tick(5 * SAMPLE_NS)
        reg(adc, DAT_AFTER_STA)
        reg(adc, CONFIG, 0)  # saradc_close
        reg(adc, CONFIG, INT_CLR)
        self.assertFalse(line.level)
        adc.tick(50 * SAMPLE_NS)
        self.assertFalse(line.level)


if __name__ == "__main__":
    unittest.main()
