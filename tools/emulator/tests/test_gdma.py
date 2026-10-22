import unittest

from core.module import BusError
from modules.gdma import GdmaModule, transfer

BASE = 0x00400000
MS = 1_000_000
INT_STATUS, PRIO_MODE = 0xE0, 0xDC


class Memory:
    """A flat RAM at BASE; anything else is a bus error."""

    def __init__(self, size=0x4000):
        self.data = bytearray(size)

    def read(self, addr, size):
        off = addr - BASE
        if off < 0 or off + size > len(self.data):
            raise BusError(addr, "unmapped")
        return bytes(self.data[off:off + size])

    def write(self, addr, data):
        off = addr - BASE
        if off < 0 or off + len(data) > len(self.data):
            raise BusError(addr, "unmapped")
        self.data[off:off + len(data)] = data

    def fill(self, addr, data):
        self.write(addr, data)


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


def config(length, enable=1, fin=1, half=0, sdw=2, ddw=2, sinc=1, dinc=1, repeat=0):
    """`length` is the byte count; the register holds count - 1."""
    return (
        enable | fin << 1 | half << 2 | repeat << 3 | sdw << 4 | ddw << 6
        | sinc << 8 | dinc << 9 | (length - 1) << 16
    )


def make(**params):
    memory, clock, line = Memory(), Clock(), Line()
    gdma = GdmaModule("GDMA", clock=clock, irq=line, memory=memory, bus_hz=120_000_000, **params)
    return gdma, memory, clock, line


def reg(gdma, offset, value=None):
    if value is None:
        return gdma.read("regs", offset, 4)
    gdma.write("regs", offset, 4, value)


def program(gdma, ch, src, dst, cfg, mux=0):
    base = ch * 0x20
    reg(gdma, base + 0x04, dst)
    reg(gdma, base + 0x08, src)
    reg(gdma, base + 0x1C, mux)
    reg(gdma, base + 0x00, cfg)


def advance(gdma, clock, ns):
    clock.now = ns
    gdma.tick(ns)


class TransferTest(unittest.TestCase):
    def run_transfer(self, source, length, **kw):
        mem = Memory()
        mem.fill(BASE, source.ljust(0x100, b"\x00"))
        mem.fill(BASE + 0x200, b"\xff" * 0x100)
        args = dict(sdw=2, ddw=2, sinc=True, dinc=True)
        args.update(kw)
        transfer(mem, BASE, BASE + 0x200, length, **args)
        return mem.read(BASE + 0x200, 0x40)

    def test_word_copy(self):
        out = self.run_transfer(bytes(range(16)), 16)
        self.assertEqual(out[:16], bytes(range(16)))
        self.assertEqual(out[16:20], b"\xff" * 4)

    def test_32_bit_source_8_bit_destination_leaves_gaps(self):
        out = self.run_transfer(bytes(range(16)), 4, ddw=0)
        self.assertEqual(out[0], 0)
        self.assertEqual(out[4], 1)
        self.assertEqual(out[8], 2)
        self.assertEqual(out[12], 3)

    def test_8_bit_source_packs_four_reads_into_one_write(self):
        source = bytes([0x11, 0, 0, 0, 0x22, 0, 0, 0, 0x33, 0, 0, 0, 0x44, 0, 0, 0])
        out = self.run_transfer(source, 4, sdw=0)
        self.assertEqual(out[:4], bytes([0x11, 0x22, 0x33, 0x44]))

    def test_16_bit_widths_move_two_bytes_per_step_of_four(self):
        out = self.run_transfer(bytes([1, 2, 0xAA, 0xAA, 3, 4, 0xAA, 0xAA]), 4, sdw=1, ddw=1)
        self.assertEqual(out[0:2], bytes([1, 2]))
        self.assertEqual(out[4:6], bytes([3, 4]))

    def test_unfilled_tail_is_committed_as_zero(self):
        out = self.run_transfer(b"\xaa" * 8, 3)
        self.assertEqual(out[:4], b"\xaa\xaa\xaa\x00")

    def test_fixed_destination_rewrites_one_address(self):
        out = self.run_transfer(bytes(range(16)), 16, dinc=False)
        self.assertEqual(out[:4], bytes([12, 13, 14, 15]))
        self.assertEqual(out[4:8], b"\xff" * 4)

    def test_fixed_source_repeats_one_word(self):
        out = self.run_transfer(bytes([9, 8, 7, 6, 1, 1, 1, 1]), 12, sinc=False)
        self.assertEqual(out[:12], bytes([9, 8, 7, 6]) * 3)

    def test_bus_error_propagates(self):
        mem = Memory(0x100)
        with self.assertRaises(BusError):
            transfer(mem, BASE, BASE + 0x80, 0x200, sdw=2, ddw=2, sinc=True, dinc=True)


class ChannelTest(unittest.TestCase):
    def test_transfer_length_register_is_bytes_minus_one(self):
        gdma, mem, clock, _ = make()
        mem.fill(BASE, bytes([0x5A, 0x5B, 0x5C, 0x5D]))
        mem.fill(BASE + 0x100, b"\xff\xff\xff\xff")
        program(gdma, 0, BASE, BASE + 0x100, config(1, sdw=0, ddw=0))
        advance(gdma, clock, MS)
        self.assertEqual(mem.read(BASE + 0x100, 4), bytes([0x5A, 0xFF, 0xFF, 0xFF]))

    def test_copy_happens_after_the_modelled_time(self):
        gdma, mem, clock, _ = make()
        mem.fill(BASE, bytes(range(64)) * 128)  # 8192 bytes
        program(gdma, 1, BASE, BASE + 0x2000, config(8192))
        self.assertTrue(reg(gdma, 0x20) & 1)
        self.assertEqual(gdma.next_event_ns(), 122_880)  # 2048 words * 7.2 cycles at 120 MHz
        advance(gdma, clock, 122_879)
        self.assertTrue(reg(gdma, 0x20) & 1)
        self.assertEqual(mem.read(BASE + 0x2000, 4), b"\x00" * 4)
        advance(gdma, clock, 122_880)
        self.assertFalse(reg(gdma, 0x20) & 1)
        self.assertEqual(mem.read(BASE + 0x2000, 64), bytes(range(64)))

    def test_intervals_from_6_up_set_the_pace(self):
        gdma, mem, clock, _ = make()
        program(gdma, 0, BASE, BASE + 0x2000, config(8192), mux=15 << 12)
        self.assertEqual(gdma.next_event_ns(), 2048 * 170 * 100_000_000 // 120_000_000)

    def test_intervals_below_6_change_nothing(self):
        gdma, mem, clock, _ = make()
        program(gdma, 0, BASE, BASE + 0x2000, config(8192), mux=5 << 12 | 5 << 16)
        self.assertEqual(gdma.next_event_ns(), 122_880)

    def test_finish_sets_flag_counter_and_irq_until_cleared(self):
        gdma, mem, clock, line = make()
        program(gdma, 2, BASE, BASE + 0x100, config(64, fin=1))
        advance(gdma, clock, MS)
        self.assertEqual(reg(gdma, INT_STATUS) & 0x3F, 1 << 2)
        self.assertTrue(line.level)
        self.assertEqual(reg(gdma, 0xC8) >> 24 & 0xF, 1)  # int_counts[2].fin_count
        reg(gdma, INT_STATUS, 1 << 2)
        self.assertEqual(reg(gdma, INT_STATUS), 0)
        self.assertFalse(line.level)
        self.assertEqual(reg(gdma, 0xC8) >> 24 & 0xF, 1)  # the counter is sticky

    def test_no_flag_or_irq_without_fin_int_enable_but_enable_clears(self):
        gdma, mem, clock, line = make()
        program(gdma, 0, BASE, BASE + 0x100, config(64, fin=0))
        advance(gdma, clock, MS)
        self.assertEqual(reg(gdma, INT_STATUS), 0)
        self.assertFalse(line.level)
        self.assertFalse(reg(gdma, 0x00) & 1)

    def test_half_finish_flag_comes_at_the_midpoint(self):
        gdma, mem, clock, line = make()
        program(gdma, 0, BASE, BASE + 0x2000, config(8192, fin=0, half=1))
        advance(gdma, clock, 61_439)
        self.assertEqual(reg(gdma, INT_STATUS), 0)
        advance(gdma, clock, 61_440)
        self.assertEqual(reg(gdma, INT_STATUS) >> 8 & 0x3F, 1)
        self.assertEqual(reg(gdma, 0xC0) >> 28 & 0xF, 1)
        self.assertTrue(line.level)
        reg(gdma, INT_STATUS, 1 << 8)
        self.assertFalse(line.level)

    def test_remain_len_counts_dst_writes_down(self):
        gdma, mem, clock, _ = make()
        program(gdma, 0, BASE, BASE + 0x2000, config(8192))
        self.assertEqual(reg(gdma, 0xC0) & 0x1FFFF, 2048)
        advance(gdma, clock, 61_440)
        self.assertEqual(reg(gdma, 0xC0) & 0x1FFFF, 1024)
        advance(gdma, clock, 122_880)
        self.assertEqual(reg(gdma, 0xC0) & 0x1FFFF, 0)

    def test_dst_wr_addr_follows_the_writes(self):
        gdma, mem, clock, _ = make()
        program(gdma, 0, BASE, BASE + 0x2000, config(8192))
        advance(gdma, clock, 61_440)
        self.assertEqual(reg(gdma, 0x160), BASE + 0x2000 + 4 * 1024)

    def test_clearing_enable_aborts_the_transfer(self):
        gdma, mem, clock, line = make()
        mem.fill(BASE, b"\x77" * 64)
        program(gdma, 0, BASE, BASE + 0x100, config(64))
        reg(gdma, 0x00, config(64, enable=0))
        self.assertIsNone(gdma.next_event_ns())
        advance(gdma, clock, MS)
        self.assertEqual(mem.read(BASE + 0x100, 4), b"\x00" * 4)
        self.assertFalse(line.level)

    def test_peripheral_request_line_stalls_with_a_warning(self):
        gdma, mem, clock, _ = make()
        with self.assertLogs("emulator.gdma", level="WARNING") as logs:
            program(gdma, 0, BASE, BASE + 0x100, config(64), mux=8)  # source JPEG
        self.assertIn("JPEG", logs.output[0])
        advance(gdma, clock, 1000 * MS)
        self.assertTrue(reg(gdma, 0x00) & 1)
        self.assertIsNone(gdma.next_event_ns())

    def test_repeat_mode_warns_and_runs_once(self):
        gdma, mem, clock, _ = make()
        with self.assertLogs("emulator.gdma", level="WARNING") as logs:
            program(gdma, 0, BASE, BASE + 0x100, config(64, repeat=1))
        self.assertIn("repeat", logs.output[0])
        advance(gdma, clock, MS)
        self.assertFalse(reg(gdma, 0x00) & 1)

    def test_bus_error_aborts_the_channel_and_says_where(self):
        gdma, mem, clock, line = make()
        program(gdma, 3, 0x20000000, BASE + 0x100, config(64))
        with self.assertLogs("emulator.gdma", level="ERROR") as logs:
            advance(gdma, clock, MS)
        self.assertIn("ch3", logs.output[0])
        self.assertIn("0x20000000", logs.output[0])
        self.assertFalse(reg(gdma, 0x60) & 1)
        self.assertFalse(line.level)

    def test_address_registers_and_priority_read_back(self):
        gdma, *_ = make()
        reg(gdma, 0x24, 0x1234)
        reg(gdma, PRIO_MODE, 1)
        self.assertEqual(reg(gdma, 0x24), 0x1234)
        self.assertEqual(reg(gdma, PRIO_MODE), 1)

    def test_reset_aborts_channels_and_clears_status(self):
        gdma, mem, clock, line = make()
        program(gdma, 0, BASE, BASE + 0x100, config(64))
        advance(gdma, clock, MS)
        program(gdma, 1, BASE, BASE + 0x100, config(64))
        gdma.reset()
        self.assertFalse(line.level)
        self.assertEqual(reg(gdma, INT_STATUS), 0)
        self.assertEqual(reg(gdma, 0xC0) >> 24, 0)
        self.assertFalse(reg(gdma, 0x20) & 1)
        self.assertIsNone(gdma.next_event_ns())


class DebugOutputTest(unittest.TestCase):
    def test_start_is_decoded(self):
        gdma, *_ = make()
        with self.assertLogs("emulator.gdma", level="DEBUG") as logs:
            program(gdma, 2, BASE + 0x100, BASE + 0x200, config(64, fin=1, half=1))
        text = "\n".join(logs.output)
        for part in (
            "ch2 start", "DTCM 0x00400100", "DTCM 0x00400200", "64 bytes",
            "32-bit", "16 writes", "fin_int", "half_fin_int",
        ):
            self.assertIn(part, text)

    def test_peripheral_lines_are_named(self):
        gdma, *_ = make()
        with self.assertLogs("emulator.gdma", level="DEBUG") as logs:
            program(gdma, 0, 0x0080A000, BASE, config(64, sinc=0), mux=8)
        self.assertIn("JPEG 0x0080A000", "\n".join(logs.output))

    def test_completion_and_acknowledge_are_decoded(self):
        gdma, mem, clock, _ = make()
        program(gdma, 4, BASE, BASE + 0x100, config(64))
        with self.assertLogs("emulator.gdma", level="DEBUG") as logs:
            advance(gdma, clock, MS)
            reg(gdma, INT_STATUS, 1 << 4)
        text = "\n".join(logs.output)
        self.assertIn("ch4 done", text)
        self.assertIn("64 bytes", text)
        self.assertIn("ch4 fin", text)

    def test_abort_is_decoded(self):
        gdma, *_ = make()
        program(gdma, 5, BASE, BASE + 0x100, config(64))
        with self.assertLogs("emulator.gdma", level="DEBUG") as logs:
            reg(gdma, 0xA0, config(64, enable=0))
        self.assertIn("ch5 stopped", "\n".join(logs.output))

    def test_clearing_the_config_is_one_short_line(self):
        gdma, *_ = make()
        with self.assertLogs("emulator.gdma", level="DEBUG") as logs:
            reg(gdma, 0x40, 0)
        self.assertEqual(len(logs.output), 1)
        self.assertIn("ch2 config cleared", logs.output[0])

    def test_config_write_without_enable_is_decoded(self):
        gdma, *_ = make()
        with self.assertLogs("emulator.gdma", level="DEBUG") as logs:
            reg(gdma, 0x00, config(16, enable=0, sdw=0, ddw=1, sinc=0))
        text = "\n".join(logs.output)
        self.assertIn("ch0 config", text)
        self.assertIn("src 8-bit fixed", text)
        self.assertIn("dst 16-bit +4", text)
        self.assertIn("16 bytes", text)


if __name__ == "__main__":
    unittest.main()
