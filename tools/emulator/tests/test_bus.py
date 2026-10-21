import struct
import unittest

from core.bus import Bus
from core.cpu import Cpu
from core.module import Fault, Module

PERIPHERALS = (0x00800000, 0x00100000)


class Recorder(Module):
    def __init__(self, name, size=0x100, **params):
        super().__init__(name, **params)
        self.size = size
        self.writes = []
        self.next_read = 0

    def ranges(self):
        return [("regs", self.size)]

    def read(self, range_name, offset, size):
        return self.next_read

    def write(self, range_name, offset, size, value):
        self.writes.append((range_name, offset, size, value))


class FaultingModule(Recorder):
    def read(self, range_name, offset, size):
        raise Fault("dabt", 0x00802104)


def make_bus():
    cpu = Cpu()
    return cpu, Bus(cpu)


def ldr_r1(cpu, value):
    """Word for `ldr r1, =value` placed first at 0x00400000; literal at 0x00400100."""
    cpu.mem_write(0x00400100, struct.pack("<I", value))
    return 0xE59F1000 | (0x100 - 8)


def run_code(cpu, *words, at=0x00400000):
    cpu.mem_write(at, struct.pack("<%dI" % len(words), *words))
    cpu.reset(at)
    return cpu.run(len(words))


class BusTest(unittest.TestCase):
    def test_memory_overlap_rejected(self):
        _, bus = make_bus()
        bus.add_memory("RAM1", 0x00400000, 0x40000)
        with self.assertRaisesRegex(ValueError, "RAM1.*RAM2|RAM2.*RAM1"):
            bus.add_memory("RAM2", 0x0043F000, 0x1000)

    def test_module_overlaps_memory_rejected(self):
        _, bus = make_bus()
        bus.add_memory("RAM1", 0x00802000, 0x1000)
        with self.assertRaisesRegex(ValueError, "RAM1"):
            bus.add_module(Recorder("UART1"), {"regs": 0x00802100})

    def test_two_modules_overlap_rejected(self):
        _, bus = make_bus()
        bus.add_module(Recorder("UART1"), {"regs": 0x00802100})
        with self.assertRaisesRegex(ValueError, "UART1.*UART2|UART2.*UART1"):
            bus.add_module(Recorder("UART2"), {"regs": 0x00802180})

    def test_mmio_dispatch_size_and_offset(self):
        cpu, bus = make_bus()
        bus.add_memory("RAM1", 0x00400000, 0x1000)
        uart = Recorder("UART1")
        bus.add_module(uart, {"regs": 0x00802100})
        bus.claim_unclaimed(*PERIPHERALS)
        # ldr r1,=0x00802102 ; mov r2,#0xAB ; strh r2,[r1]
        run_code(cpu, ldr_r1(cpu, 0x00802102), 0xE3A020AB, 0xE1C120B0)
        self.assertEqual(uart.writes, [("regs", 2, 2, 0xAB)])

    def test_unclaimed_peripheral_read_zero_logged_once(self):
        cpu, bus = make_bus()
        bus.add_memory("RAM1", 0x00400000, 0x1000)
        bus.claim_unclaimed(*PERIPHERALS)
        # ldr r1,=0x00805000 ; ldr r0,[r1] ; ldr r2,[r1]
        with self.assertLogs("emulator.bus", level="WARNING") as logs:
            run_code(cpu, ldr_r1(cpu, 0x00805000), 0xE5910000, 0xE5912000)
        self.assertEqual(cpu.reg("r0"), 0)
        self.assertEqual(len(logs.output), 1)
        self.assertIn("0x00805000", logs.output[0])

    def test_module_fault_reaches_sink(self):
        cpu, bus = make_bus()
        bus.add_memory("RAM1", 0x00400000, 0x1000)
        bus.add_module(FaultingModule("F"), {"regs": 0x00802100})
        bus.claim_unclaimed(*PERIPHERALS)
        faults = []
        bus.set_fault_sink(faults.append)
        run_code(cpu, ldr_r1(cpu, 0x00802104), 0xE5910000)
        self.assertEqual(len(faults), 1)
        self.assertEqual((faults[0].kind, faults[0].addr), ("dabt", 0x00802104))


class UnmappedTest(unittest.TestCase):
    def machine(self):
        cpu, bus = make_bus()
        bus.add_memory("RAM1", 0x00400000, 0x1000)
        bus.claim_unclaimed(*PERIPHERALS)
        bus.set_fault_sink(lambda fault: cpu.post_fault(fault.kind))
        return cpu

    def test_unmapped_read_is_data_abort_with_one_warning_per_page(self):
        cpu = self.machine()
        with self.assertLogs("emulator.bus", level="WARNING") as logs:
            for _ in range(2):
                run_code(cpu, ldr_r1(cpu, 0x20000010), 0xE5910000)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x17)
        self.assertEqual(cpu.reg("lr"), 0x00400004 + 8)
        self.assertEqual(len(logs.output), 1)
        self.assertIn("0x20000010", logs.output[0])

    def test_unmapped_write_is_data_abort(self):
        cpu = self.machine()
        with self.assertLogs("emulator.bus", level="WARNING"):
            run_code(cpu, ldr_r1(cpu, 0x20000000), 0xE5810000)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x17)

    def test_unmapped_fetch_is_prefetch_abort(self):
        cpu = self.machine()
        cpu.reset(0x20000000)
        with self.assertLogs("emulator.bus", level="WARNING"):
            cpu.run(1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x17)
        self.assertEqual(cpu.reg("lr"), 0x20000000 + 4)
        self.assertEqual(cpu.reg("pc"), 0x0C)


MOV_R0_5, ADD_R0_7, B_SELF = 0xE3A00005, 0xE2800007, 0xEAFFFFFE
LDR_R0_R1 = 0xE5910000
STR_R2_R1 = 0xE5812000
MOV_R2_9 = 0xE3A02009


class DemandModule(Module):
    """Serves 4 KiB pages; blocks listed in `bad` are 32-byte units that fault."""

    demand_ranges = ("win",)

    def __init__(self, name, pages, bad=()):
        super().__init__(name)
        self.pages = pages  # page offset -> bytes
        self.bad = set(bad)  # byte offsets (multiples of 32) that are bad
        self.invalidate = lambda range_name, offset, size: None

    def ranges(self):
        return [("win", 0x4000)]

    def page(self, range_name, offset):
        data = self.pages.get(offset, bytes(0x1000))
        bad = [(b - offset, b - offset + 32) for b in sorted(self.bad)
               if offset <= b < offset + 0x1000]
        return data, bad

    def read(self, range_name, offset, size):
        if offset - offset % 32 in self.bad:
            raise Fault("dabt", offset)
        return 0


def code(*words):
    return struct.pack("<%dI" % len(words), *words).ljust(0x1000, b"\0")


def make_demand_machine(module):
    cpu, bus = make_bus()
    bus.add_memory("RAM1", 0x00400000, 0x1000)
    bus.add_module(module, {"win": 0x00000000})
    bus.claim_unclaimed(*PERIPHERALS)
    bus.set_fault_sink(lambda fault: cpu.post_fault(fault.kind))
    return cpu, bus


class DemandRangeTest(unittest.TestCase):
    def test_good_page_is_readable_and_executable(self):
        module = DemandModule("W", {0: code(MOV_R0_5, ADD_R0_7, B_SELF)})
        cpu, _ = make_demand_machine(module)
        cpu.reset(0)
        cpu.run(3)
        self.assertEqual(cpu.reg("r0"), 12)

    def test_data_read_of_bad_block_aborts(self):
        module = DemandModule("W", {}, bad=[0x1000])
        cpu, _ = make_demand_machine(module)
        run_code(cpu, ldr_r1(cpu, 0x1000), LDR_R0_R1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x17)
        self.assertEqual(cpu.reg("lr"), 0x00400004 + 8)
        self.assertEqual(cpu.reg("pc"), 0x10)

    def test_fetch_from_bad_block_is_prefetch_abort(self):
        module = DemandModule("W", {}, bad=[0x0])
        cpu, _ = make_demand_machine(module)
        cpu.reset(0)
        cpu.run(1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x17)
        self.assertEqual(cpu.reg("pc"), 0x0C)

    def test_mixed_page_good_block_readable_bad_block_aborts(self):
        module = DemandModule("W", {0x1000: bytes([7]) * 0x1000}, bad=[0x1020])
        cpu, _ = make_demand_machine(module)
        run_code(cpu, ldr_r1(cpu, 0x1010), LDR_R0_R1)
        self.assertEqual(cpu.reg("r0"), 0x07070707)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x13)
        run_code(cpu, ldr_r1(cpu, 0x1020), LDR_R0_R1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x17)

    def test_fetch_from_bad_block_in_mixed_page_is_prefetch_abort(self):
        module = DemandModule("W", {0: code(MOV_R0_5, B_SELF)}, bad=[0x20])
        cpu, _ = make_demand_machine(module)
        cpu.reset(0)
        cpu.run(2)
        self.assertEqual(cpu.reg("r0"), 5)
        cpu.set_reg("pc", 0x20)
        cpu.run(1)
        self.assertEqual(cpu.reg("pc"), 0x0C)

    def test_write_is_ignored_with_one_warning(self):
        module = DemandModule("W", {0x1000: bytes([7]) * 0x1000})
        cpu, _ = make_demand_machine(module)
        with self.assertLogs("emulator.bus", level="WARNING") as logs:
            run_code(cpu, ldr_r1(cpu, 0x1000), MOV_R2_9, STR_R2_R1, STR_R2_R1)
        self.assertEqual(len(logs.output), 1)
        self.assertEqual(cpu.mem_read(0x1000, 4), b"\x07\x07\x07\x07")

    def test_invalidate_reloads_page(self):
        module = DemandModule("W", {0x1000: bytes([7]) * 0x1000})
        cpu, _ = make_demand_machine(module)
        run_code(cpu, ldr_r1(cpu, 0x1000), LDR_R0_R1)
        self.assertEqual(cpu.reg("r0"), 0x07070707)
        module.pages[0x1000] = bytes([9]) * 0x1000
        module.invalidate("win", 0x1000, 0x1000)
        run_code(cpu, ldr_r1(cpu, 0x1000), LDR_R0_R1)
        self.assertEqual(cpu.reg("r0"), 0x09090909)


if __name__ == "__main__":
    unittest.main()
