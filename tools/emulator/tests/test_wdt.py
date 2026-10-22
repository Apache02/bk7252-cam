import unittest

from modules.wdt import WdtModule

KEY1, KEY2 = 0x5A << 16, 0xA5 << 16
MS = 1_000_000


class Clock:
    def __init__(self):
        self.now = 0

    def now_ns(self):
        return self.now


class Gate:
    def __init__(self, off=False):
        self.off = off

    def active(self):
        return self.off


def make(off=False):
    clock, gate, resets = Clock(), Gate(off), []
    wdt = WdtModule("WDT", clock=clock, clock_off=gate, reset_chip=lambda: resets.append(clock.now))
    return wdt, clock, gate, resets


def write(wdt, value):
    wdt.write("regs", 0, 4, value)


def kick(wdt, period):
    write(wdt, KEY1 | period)
    write(wdt, KEY2 | period)


def advance(wdt, clock, ns):
    clock.now = ns
    wdt.tick(ns)


class WdtTest(unittest.TestCase):
    def test_reset_value_is_0x4000(self):
        wdt, *_ = make()
        self.assertEqual(wdt.read("regs", 0, 4), 0x4000)

    def test_ctrl_reads_back_last_write_including_wrong_key(self):
        wdt, *_ = make()
        write(wdt, 777)
        self.assertEqual(wdt.read("regs", 0, 4), 777)
        kick(wdt, 2000)
        self.assertEqual(wdt.read("regs", 0, 4), KEY2 | 2000)

    def test_expires_after_period_milliseconds(self):
        wdt, clock, _, resets = make()
        kick(wdt, 2000)
        advance(wdt, clock, 1999 * MS)
        self.assertEqual(resets, [])
        advance(wdt, clock, 2000 * MS)
        self.assertEqual(resets, [2000 * MS])

    def test_next_event_is_the_deadline(self):
        wdt, clock, _, _ = make()
        kick(wdt, 2000)
        wdt.tick(0)
        self.assertEqual(wdt.next_event_ns(), 2000 * MS)

    def test_two_key_write_postpones_expiry(self):
        wdt, clock, _, resets = make()
        kick(wdt, 2000)
        advance(wdt, clock, 1500 * MS)
        kick(wdt, 2000)
        advance(wdt, clock, 3499 * MS)
        self.assertEqual(resets, [])
        advance(wdt, clock, 3500 * MS)
        self.assertEqual(len(resets), 1)

    def test_wrong_key_write_does_not_reload(self):
        wdt, clock, _, resets = make()
        kick(wdt, 2000)
        advance(wdt, clock, 1500 * MS)
        write(wdt, 2000)
        write(wdt, KEY2 | 2000)  # second key without the first
        advance(wdt, clock, 2000 * MS)
        self.assertEqual(len(resets), 1)

    def test_stopped_while_the_clock_is_powered_down(self):
        wdt, clock, _, resets = make(off=True)
        kick(wdt, 2000)
        advance(wdt, clock, 1_000_000 * MS)
        self.assertEqual(resets, [])
        self.assertIsNone(wdt.next_event_ns())

    def test_countdown_starts_when_the_clock_comes_on(self):
        wdt, clock, gate, resets = make(off=True)
        kick(wdt, 2000)
        advance(wdt, clock, 5000 * MS)
        gate.off = False
        advance(wdt, clock, 5000 * MS + 1)
        advance(wdt, clock, 6999 * MS)
        self.assertEqual(resets, [])
        advance(wdt, clock, 7001 * MS)
        self.assertEqual(len(resets), 1)

    def test_default_period_counts_16_seconds(self):
        wdt, clock, _, resets = make()
        advance(wdt, clock, 0)
        advance(wdt, clock, 16_383 * MS)
        self.assertEqual(resets, [])
        advance(wdt, clock, 16_385 * MS)
        self.assertEqual(len(resets), 1)

    def test_reset_restores_default_and_stops_countdown(self):
        wdt, clock, _, resets = make()
        kick(wdt, 100)
        wdt.tick(0)
        wdt.reset()
        self.assertEqual(wdt.read("regs", 0, 4), 0x4000)
        self.assertIsNone(wdt.next_event_ns())


if __name__ == "__main__":
    unittest.main()
