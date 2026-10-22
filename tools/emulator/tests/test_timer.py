import unittest

from modules.timer import TimerModule

CTL = 0x0C
EN0, EN1 = 1, 2
STATUS_SHIFT = 7
MS = 1_000_000


class Clock:
    def __init__(self):
        self.now = 0

    def now_ns(self):
        return self.now


class Line:
    def __init__(self):
        self.level = False

    def set(self, level):
        self.level = level


def make(hz=26_000_000):
    clock, line = Clock(), Line()
    timer = TimerModule("TIMER0", clock=clock, irq=line, hz=hz)
    return timer, clock, line


def reg(timer, offset, value=None):
    if value is None:
        return timer.read("regs", offset, 4)
    timer.write("regs", offset, 4, value)


def status(timer):
    return (reg(timer, CTL) >> STATUS_SHIFT) & 7


class TimerTest(unittest.TestCase):
    def test_periodic_fires_after_period(self):
        timer, clock, line = make()
        reg(timer, 0x00, 26_000)  # 1 ms at 26 MHz
        reg(timer, CTL, EN0)
        clock.now = 999_999
        timer.tick(clock.now)
        self.assertFalse(line.level)
        clock.now = MS
        timer.tick(clock.now)
        self.assertTrue(line.level)
        self.assertEqual(status(timer), 1)

    def test_write_one_clears_status_and_line(self):
        timer, clock, line = make()
        reg(timer, 0x00, 26_000)
        reg(timer, CTL, EN0)
        clock.now = MS
        timer.tick(clock.now)
        reg(timer, CTL, EN0 | (1 << STATUS_SHIFT))
        self.assertEqual(status(timer), 0)
        self.assertFalse(line.level)

    def test_writing_zero_status_bits_keeps_pending_flags(self):
        timer, clock, _ = make()
        reg(timer, 0x00, 26_000)
        reg(timer, CTL, EN0)
        clock.now = MS
        timer.tick(clock.now)
        reg(timer, CTL, EN0 | (3 << 3))  # change the divider only
        self.assertEqual(status(timer), 1)

    def test_fires_again_each_period(self):
        timer, clock, _ = make()
        reg(timer, 0x00, 26_000)
        reg(timer, CTL, EN0)
        for n in (1, 2, 3):
            clock.now = n * MS
            timer.tick(clock.now)
            self.assertEqual(status(timer), 1)
            reg(timer, CTL, EN0 | (1 << STATUS_SHIFT))
            self.assertEqual(status(timer), 0)

    def test_divider_scales_period(self):
        timer, clock, _ = make()
        reg(timer, 0x00, 26_000)
        reg(timer, CTL, EN0 | (1 << 3))  # divide by 2
        clock.now = MS
        timer.tick(clock.now)
        self.assertEqual(status(timer), 0)
        clock.now = 2 * MS
        timer.tick(clock.now)
        self.assertEqual(status(timer), 1)

    def test_disabled_timer_does_not_fire(self):
        timer, clock, line = make()
        reg(timer, 0x00, 26_000)
        clock.now = 10 * MS
        timer.tick(clock.now)
        self.assertFalse(line.level)
        self.assertIsNone(timer.next_event_ns())

    def test_enable_starts_counting_from_now(self):
        timer, clock, _ = make()
        reg(timer, 0x00, 26_000)
        clock.now = 5 * MS
        reg(timer, CTL, EN0)
        self.assertEqual(timer.next_event_ns(), 6 * MS)

    def test_next_event_is_the_earliest_enabled_timer(self):
        timer, clock, _ = make()
        reg(timer, 0x00, 52_000)
        reg(timer, 0x04, 26_000)
        reg(timer, CTL, EN0 | EN1)
        self.assertEqual(timer.next_event_ns(), MS)

    def test_long_gap_sets_flag_once_and_schedules_ahead(self):
        timer, clock, _ = make()
        reg(timer, 0x00, 26_000)
        reg(timer, CTL, EN0)
        clock.now = 10 * MS + 1
        timer.tick(clock.now)
        self.assertEqual(status(timer), 1)
        self.assertGreater(timer.next_event_ns(), clock.now)

    def test_32khz_bank_period(self):
        timer, clock, _ = make(hz=32_000)
        reg(timer, 0x00, 32)  # 1 ms
        reg(timer, CTL, EN0)
        clock.now = MS
        timer.tick(clock.now)
        self.assertEqual(status(timer), 1)

    def test_reset_stops_timers_and_clears_status(self):
        timer, clock, line = make()
        reg(timer, 0x00, 26_000)
        reg(timer, CTL, EN0)
        clock.now = MS
        timer.tick(clock.now)
        self.assertTrue(line.level)
        timer.reset()
        self.assertFalse(line.level)
        self.assertIsNone(timer.next_event_ns())
        self.assertEqual(reg(timer, CTL), 0)
        self.assertEqual(reg(timer, 0x00), 0)

    def test_counter_registers_read_back(self):
        timer, _, _ = make()
        reg(timer, 0x08, 1234)
        self.assertEqual(reg(timer, 0x08), 1234)


if __name__ == "__main__":
    unittest.main()
