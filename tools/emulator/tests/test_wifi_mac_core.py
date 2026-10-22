import unittest

from modules.wifi import MacCore

COUNTER_1, COUNTER_2_LO, COUNTER_2_HI, ABS_TIMER = 0x11C, 0x120, 0x124, 0x128
US = 1000
MS = 1_000_000


class Clock:
    def __init__(self):
        self.now = 0

    def now_ns(self):
        return self.now


class Power:
    def __init__(self, up):
        self.up = up

    def active(self):
        return self.up


def make(power=None):
    clock = Clock()
    params = {} if power is None else {"mac_powered": power}
    return MacCore("MAC_CORE", clock=clock, **params), clock


def at(core, clock, ns):
    clock.now = ns
    core.tick(ns)


def reg(core, offset):
    return core.read("regs", offset, 4)


class CountersTest(unittest.TestCase):
    def test_all_three_read_zero_while_the_mac_is_powered_down(self):
        core, clock = make(Power(False))
        at(core, clock, 5 * MS)
        self.assertEqual([reg(core, o) for o in (COUNTER_1, COUNTER_2_LO, COUNTER_2_HI)], [0, 0, 0])

    def test_counter_1_runs_at_40_mhz_from_power_up(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        at(core, clock, 1 * US)
        self.assertEqual(reg(core, COUNTER_1), 40)
        at(core, clock, 1 * MS)
        self.assertEqual(reg(core, COUNTER_1), 40_000)

    def test_counter_1_wraps_at_32_bits(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        at(core, clock, 2**32 // 40 * US + 25 * US)  # 2**32 ticks and a bit
        self.assertLess(reg(core, COUNTER_1), 2**32)
        self.assertEqual(reg(core, COUNTER_1), (2**32 // 40 * 40 + 1000) % 2**32)

    def test_counter_2_counts_microseconds_in_lo_and_hi(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        at(core, clock, 1500 * US)
        self.assertEqual(reg(core, COUNTER_2_LO), 1500)
        self.assertEqual(reg(core, COUNTER_2_HI), 0)
        at(core, clock, (2**32 + 7) * US)
        self.assertEqual(reg(core, COUNTER_2_LO), 7)
        self.assertEqual(reg(core, COUNTER_2_HI), 1)

    def test_hi_keeps_the_software_update_bit_clear(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        at(core, clock, (3 * 2**32) * US)
        self.assertEqual(reg(core, COUNTER_2_HI), 3)

    def test_counters_start_when_the_power_comes_on(self):
        power = Power(False)
        core, clock = make(power)
        at(core, clock, 5 * MS)
        power.up = True
        at(core, clock, 6 * MS)
        at(core, clock, 7 * MS)
        self.assertEqual(reg(core, COUNTER_2_LO), 1000)

    def test_losing_power_zeroes_them_and_the_next_power_up_starts_again(self):
        power = Power(True)
        core, clock = make(power)
        at(core, clock, 0)
        at(core, clock, 2 * MS)
        power.up = False
        at(core, clock, 3 * MS)
        self.assertEqual(reg(core, COUNTER_2_LO), 0)
        power.up = True
        at(core, clock, 10 * MS)
        at(core, clock, 11 * MS)
        self.assertEqual(reg(core, COUNTER_2_LO), 1000)

    def test_without_a_power_signal_they_run_from_the_start_of_time(self):
        core, clock = make()
        clock.now = 1 * MS
        self.assertEqual(reg(core, COUNTER_2_LO), 1000)

    def test_a_read_sees_the_time_of_the_read_not_of_the_last_tick(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        clock.now = 2 * MS
        self.assertEqual(reg(core, COUNTER_2_LO), 2000)

    def test_software_update_loads_counter_2_from_lo_and_hi(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        at(core, clock, 10 * US)
        core.write("regs", COUNTER_2_LO, 4, 0x100)
        core.write("regs", COUNTER_2_HI, 4, 1 << 31 | 0x2)
        self.assertEqual(reg(core, COUNTER_2_LO), 0x100)
        self.assertEqual(reg(core, COUNTER_2_HI), 0x2)
        at(core, clock, 15 * US)
        self.assertEqual(reg(core, COUNTER_2_LO), 0x105)

    def test_writing_hi_without_the_update_bit_changes_nothing(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        at(core, clock, 10 * US)
        core.write("regs", COUNTER_2_HI, 4, 0x5)
        self.assertEqual(reg(core, COUNTER_2_HI), 0)
        self.assertEqual(reg(core, COUNTER_2_LO), 10)

    def test_counter_1_ignores_writes(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        at(core, clock, 1 * US)
        core.write("regs", COUNTER_1, 4, 0xFFFFFFFF)
        self.assertEqual(reg(core, COUNTER_1), 40)

    def test_the_rest_of_the_block_is_plain_memory(self):
        core, _ = make(Power(True))
        core.write("regs", ABS_TIMER, 4, 0xCAFE)
        self.assertEqual(reg(core, ABS_TIMER), 0xCAFE)
        self.assertEqual(core.read("regs", ABS_TIMER, 2), 0xCAFE)

    def test_reset_restarts_the_counters_and_clears_the_memory(self):
        core, clock = make(Power(True))
        at(core, clock, 0)
        core.write("regs", ABS_TIMER, 4, 7)
        at(core, clock, 5 * MS)
        core.reset()
        self.assertEqual(reg(core, ABS_TIMER), 0)
        at(core, clock, 6 * MS)
        at(core, clock, 7 * MS)
        self.assertEqual(reg(core, COUNTER_2_LO), 1000)


if __name__ == "__main__":
    unittest.main()
