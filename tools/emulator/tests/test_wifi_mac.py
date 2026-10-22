import unittest

from modules.wifi.mac import Mac

# core offsets
VERSION_1, STATE_CNTRL, BCN_CNTRL_1, ENCR_KEY0, ENCR_ADDR_LO, ENCR_ADDR_HI, ENCR_CNTRL = (
    0x004, 0x038, 0x064, 0x0AC, 0x0BC, 0x0C0, 0x0C4)
MONO_LO, ABS_TIMER0 = 0x120, 0x128
# pl offsets
GEN_STATUS, GEN_ACK, GEN_ENABLE = 0x06C, 0x070, 0x074
TXRX_STATUS, TXRX_ACK, TXRX_ENABLE = 0x078, 0x07C, 0x080
EVENT_SET, EVENT_CLEAR, UNMASK = 0x084, 0x088, 0x08C
TSF_LO, TSF_HI = 0x0A4, 0x0A8
TOA_PARAM_1, TOA_PARAM_2, TOA_VALUE = 0x160, 0x164, 0x168

MASTER = 1 << 31
IDLE_INT, TIMERS_INT, TBTT_INT = 1 << 2, 1 << 3, 1 << 0
US = 1000


class Clock:
    def __init__(self):
        self.now = 0

    def now_ns(self):
        return self.now


class Power:
    def __init__(self, on=True):
        self.on = on

    def active(self):
        return self.on


class Line:
    def __init__(self):
        self.level = False

    def set(self, level):
        self.level = level


def make(on=True):
    clock, power = Clock(), Power(on)
    lines = {n: Line() for n in ("general", "tx_trigger", "rx_trigger", "prot_trigger", "tx_rx_timer", "tx_rx_misc")}
    mac = Mac("MAC", clock=clock, mac_powered=power, irq=lines)
    mac.tick(0)
    return mac, clock, power, lines


def core(mac, offset, value=None):
    if value is None:
        return mac.read("core", offset, 4)
    mac.write("core", offset, 4, value)


def pl(mac, offset, value=None):
    if value is None:
        return mac.read("pl", offset, 4)
    mac.write("pl", offset, 4, value)


def advance(mac, clock, us):
    clock.now = us * US
    mac.tick(clock.now)


class RangesTest(unittest.TestCase):
    def test_core_and_pl_are_two_ranges(self):
        sizes = dict(make()[0].ranges())
        self.assertEqual(sizes["core"], 1376 * 4)
        self.assertEqual(sizes["pl"], 351 * 4)

    def test_plain_registers_read_back(self):
        mac, *_ = make()
        core(mac, 0x10, 0xC8478C72)
        pl(mac, 0x100, 0x1234)
        self.assertEqual((core(mac, 0x10), pl(mac, 0x100)), (0xC8478C72, 0x1234))

    def test_version_register(self):
        self.assertEqual(core(make()[0], VERSION_1), 0x00001C03)


class StateTest(unittest.TestCase):
    def test_next_state_is_taken_at_once(self):
        mac, *_ = make()
        core(mac, STATE_CNTRL, 3 << 4)
        self.assertEqual(core(mac, STATE_CNTRL), (3 << 4) | 3)

    def test_leaving_active_for_idle_raises_the_idle_interrupt(self):
        mac, _, _, lines = make()
        pl(mac, GEN_ENABLE, MASTER | IDLE_INT)
        core(mac, STATE_CNTRL, 3 << 4)
        self.assertFalse(lines["general"].level)
        core(mac, STATE_CNTRL, 0)
        self.assertEqual(pl(mac, GEN_STATUS), IDLE_INT)
        self.assertTrue(lines["general"].level)

    def test_acknowledge_clears_status_and_line(self):
        mac, _, _, lines = make()
        pl(mac, GEN_ENABLE, MASTER | IDLE_INT)
        core(mac, STATE_CNTRL, 3 << 4)
        core(mac, STATE_CNTRL, 0)
        pl(mac, GEN_ACK, IDLE_INT)
        self.assertEqual(pl(mac, GEN_STATUS), 0)
        self.assertFalse(lines["general"].level)

    def test_status_shows_only_enabled_bits_and_the_line_needs_the_master_enable(self):
        mac, _, _, lines = make()
        core(mac, STATE_CNTRL, 3 << 4)
        core(mac, STATE_CNTRL, 0)
        self.assertEqual(pl(mac, GEN_STATUS), 0)
        pl(mac, GEN_ENABLE, IDLE_INT)
        self.assertEqual(pl(mac, GEN_STATUS), IDLE_INT)
        self.assertFalse(lines["general"].level)


class AbsTimerTest(unittest.TestCase):
    def arm(self, mac, index, target):
        core(mac, ABS_TIMER0 + 4 * index, target)

    def test_an_armed_timer_fires_when_the_counter_reaches_it(self):
        mac, clock, _, lines = make()
        pl(mac, UNMASK, 1)
        pl(mac, GEN_ENABLE, MASTER | TIMERS_INT)
        self.arm(mac, 0, 100)
        advance(mac, clock, 99)
        self.assertEqual(pl(mac, EVENT_SET), 0)
        advance(mac, clock, 100)
        self.assertEqual(pl(mac, EVENT_SET), 1)
        self.assertEqual(pl(mac, GEN_STATUS), TIMERS_INT)
        self.assertTrue(lines["general"].level)

    def test_a_timer_fires_once(self):
        mac, clock, *_ = make()
        pl(mac, UNMASK, 1)
        self.arm(mac, 0, 100)
        advance(mac, clock, 100)
        pl(mac, EVENT_CLEAR, 1)
        advance(mac, clock, 500)
        self.assertEqual(pl(mac, EVENT_SET), 0)

    def test_a_target_already_behind_the_counter_is_not_armed(self):
        mac, clock, *_ = make()
        pl(mac, UNMASK, 1)
        advance(mac, clock, 200)
        self.arm(mac, 0, 100)
        advance(mac, clock, 400)
        self.assertEqual(pl(mac, EVENT_SET), 0)

    def test_a_masked_timer_expires_silently(self):
        mac, clock, *_ = make()
        self.arm(mac, 0, 100)
        advance(mac, clock, 150)
        pl(mac, UNMASK, 1)
        self.assertEqual(pl(mac, EVENT_SET), 0)

    def test_software_can_set_an_event_by_hand(self):
        mac, _, _, lines = make()
        pl(mac, UNMASK, 4)
        pl(mac, GEN_ENABLE, MASTER | TIMERS_INT)
        pl(mac, EVENT_SET, 4)
        self.assertEqual(pl(mac, EVENT_SET), 4)
        self.assertTrue(lines["general"].level)
        pl(mac, EVENT_CLEAR, 4)
        self.assertEqual(pl(mac, EVENT_SET), 0)

    def test_the_next_event_is_the_earliest_armed_timer(self):
        mac, clock, *_ = make()
        pl(mac, UNMASK, 3)
        self.arm(mac, 0, 500)
        self.arm(mac, 1, 300)
        self.assertEqual(mac.next_event_ns(), 300 * US)
        advance(mac, clock, 300)
        self.assertEqual(mac.next_event_ns(), 500 * US)
        advance(mac, clock, 500)
        self.assertIsNone(mac.next_event_ns())

    def test_nothing_fires_while_the_mac_is_powered_down(self):
        mac, clock, _, _ = make(on=False)
        pl(mac, UNMASK, 1)
        self.arm(mac, 0, 100)
        advance(mac, clock, 1000)
        self.assertEqual(pl(mac, EVENT_SET), 0)
        self.assertIsNone(mac.next_event_ns())


class TsfTest(unittest.TestCase):
    def test_tsf_follows_the_microsecond_counter(self):
        mac, clock, *_ = make()
        advance(mac, clock, 250)
        self.assertEqual(pl(mac, TSF_LO), 250)
        self.assertEqual(pl(mac, TSF_HI), 0)

    def test_software_can_set_the_tsf(self):
        mac, clock, *_ = make()
        advance(mac, clock, 100)
        pl(mac, TSF_HI, 2)
        pl(mac, TSF_LO, 1000)
        advance(mac, clock, 150)
        self.assertEqual(pl(mac, TSF_LO), 1050)
        self.assertEqual(pl(mac, TSF_HI), 2)


class TbttTest(unittest.TestCase):
    def test_every_beacon_interval_raises_the_tbtt_interrupt_while_active(self):
        mac, clock, _, lines = make()
        pl(mac, GEN_ENABLE, MASTER | TBTT_INT)
        core(mac, BCN_CNTRL_1, 100)  # 100 TU = 102400 us
        core(mac, STATE_CNTRL, 3 << 4)
        advance(mac, clock, 100_000)
        self.assertEqual(pl(mac, GEN_STATUS), 0)
        advance(mac, clock, 102_400)
        self.assertEqual(pl(mac, GEN_STATUS), TBTT_INT)
        self.assertTrue(lines["general"].level)

    def test_the_next_event_includes_the_next_tbtt(self):
        mac, clock, *_ = make()
        core(mac, BCN_CNTRL_1, 100)
        core(mac, STATE_CNTRL, 3 << 4)
        advance(mac, clock, 1000)
        self.assertEqual(mac.next_event_ns(), 102_400 * US)


class KeyRamTest(unittest.TestCase):
    def write_key(self, mac, index, addr_lo, key0, ctype=4):
        core(mac, ENCR_ADDR_LO, addr_lo)
        core(mac, ENCR_ADDR_HI, 0x1234)
        core(mac, ENCR_KEY0, key0)
        core(mac, ENCR_CNTRL, 1 << 30 | index << 16 | ctype << 8)

    def test_a_command_completes_at_once(self):
        mac, *_ = make()
        self.write_key(mac, 5, 0xAABBCCDD, 0x11111111)
        self.assertEqual(core(mac, ENCR_CNTRL) & (7 << 29), 0)

    def test_a_written_key_reads_back(self):
        mac, *_ = make()
        self.write_key(mac, 5, 0xAABBCCDD, 0x11111111)
        core(mac, ENCR_KEY0, 0)
        core(mac, ENCR_ADDR_LO, 0)
        core(mac, ENCR_CNTRL, 1 << 31 | 5 << 16)
        self.assertEqual(core(mac, ENCR_KEY0), 0x11111111)
        self.assertEqual(core(mac, ENCR_ADDR_LO), 0xAABBCCDD)
        self.assertEqual(core(mac, ENCR_CNTRL) >> 8 & 7, 4)

    def test_search_finds_the_index_or_flags_an_error(self):
        mac, *_ = make()
        self.write_key(mac, 7, 0xAABBCCDD, 1)
        core(mac, ENCR_CNTRL, 1 << 29)
        self.assertEqual(core(mac, ENCR_CNTRL) >> 16 & 0x3FF, 7)
        self.assertEqual(core(mac, ENCR_CNTRL) & (1 << 28), 0)
        core(mac, ENCR_ADDR_LO, 0x01020304)
        core(mac, ENCR_CNTRL, 1 << 29)
        self.assertTrue(core(mac, ENCR_CNTRL) & (1 << 28))


class TimeOnAirTest(unittest.TestCase):
    def test_compute_duration_sets_the_valid_bit_and_a_time(self):
        mac, *_ = make()
        pl(mac, TOA_PARAM_1, 100)  # 100 bytes, non-HT
        pl(mac, TOA_PARAM_2, 4)  # 6 Mbit/s
        pl(mac, TOA_VALUE, 1 << 31)
        value = pl(mac, TOA_VALUE)
        self.assertTrue(value & (1 << 30))
        self.assertFalse(value & (1 << 31))
        self.assertEqual(value & 0xFFFF, 20 + 100 * 8 // 6)


class ResetTest(unittest.TestCase):
    def test_reset_clears_state_and_interrupts(self):
        mac, _, _, lines = make()
        pl(mac, GEN_ENABLE, MASTER | IDLE_INT)
        core(mac, STATE_CNTRL, 3 << 4)
        core(mac, STATE_CNTRL, 0)
        mac.reset()
        self.assertEqual(core(mac, STATE_CNTRL), 0)
        self.assertEqual(pl(mac, GEN_STATUS), 0)
        self.assertFalse(lines["general"].level)


if __name__ == "__main__":
    unittest.main()
