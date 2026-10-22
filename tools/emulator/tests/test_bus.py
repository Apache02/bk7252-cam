import struct
import unittest

from core.bus import Bus
from core.cpu import Cpu
from core.module import BusError, Fault, Module

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
    cpu.unicorn.ctl_flush_tb()  # code rewritten from the host is not noticed by cached blocks
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


class OutsideWindowTest(unittest.TestCase):
    """Blocks far from the peripheral window, several to one 4 KiB page (the WiFi blocks)."""

    def machine(self):
        cpu, bus = make_bus()
        bus.add_memory("RAM1", 0x00400000, 0x1000)
        low, high = Recorder("MDM_STAT", size=0x6C), Recorder("MDM_CFG", size=0x98)
        bus.add_module(low, {"regs": 0x01000000})
        bus.add_module(high, {"regs": 0x01000800})
        bus.add_module(Recorder("INTC", size=0x44), {"regs": 0x10910000})
        bus.claim_unclaimed(*PERIPHERALS)
        bus.set_fault_sink(lambda fault: cpu.post_fault(fault.kind))
        return cpu, low, high

    def store(self, cpu, address, value):
        run_code(cpu, ldr_r1(cpu, address), 0xE3A02000 | value, STR_R2_R1)

    def test_two_modules_in_one_page_get_their_own_offsets(self):
        cpu, low, high = self.machine()
        self.store(cpu, 0x01000010, 0x11)
        self.store(cpu, 0x01000804, 0x22)
        self.assertEqual(low.writes, [("regs", 0x10, 4, 0x11)])
        self.assertEqual(high.writes, [("regs", 0x04, 4, 0x22)])

    def test_unaligned_base_and_size_are_fine(self):
        cpu, bus = make_bus()
        bus.add_memory("RAM1", 0x00400000, 0x1000)
        intc = Recorder("INTC", size=0x44)
        bus.add_module(intc, {"regs": 0x10910000})
        bus.claim_unclaimed(*PERIPHERALS)
        run_code(cpu, ldr_r1(cpu, 0x10910040), LDR_R0_R1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x13)  # no abort

    def test_gap_between_modules_in_a_mapped_page_is_unclaimed_and_reads_zero(self):
        cpu, _, _ = self.machine()
        with self.assertLogs("emulator.bus", level="WARNING") as logs:
            run_code(cpu, ldr_r1(cpu, 0x01000400), LDR_R0_R1)
        self.assertEqual(cpu.reg("r0"), 0)
        self.assertIn("0x01000400", logs.output[0])
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x13)

    def test_read_reaches_the_module(self):
        cpu, low, _ = self.machine()
        low.next_read = 0x1234
        run_code(cpu, ldr_r1(cpu, 0x01000000), LDR_R0_R1)
        self.assertEqual(cpu.reg("r0"), 0x1234)

    def test_page_outside_every_block_is_still_a_bus_error(self):
        cpu, _, _ = self.machine()
        with self.assertLogs("emulator.bus", level="WARNING"):
            run_code(cpu, ldr_r1(cpu, 0x01005000), LDR_R0_R1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x17)


class BusMemoryTest(unittest.TestCase):
    """The bus-master view of memory that DMA engines use."""

    def machine(self, module=None):
        cpu, bus = make_bus()
        bus.add_memory("RAM1", 0x00400000, 0x2000)
        bus.add_memory("RAM2", 0x00402000, 0x1000)  # adjacent to RAM1
        if module is not None:
            bus.add_module(module, {"win": 0x00000000})
        bus.claim_unclaimed(*PERIPHERALS)
        return bus.memory()

    def test_reads_and_writes_ram_across_a_page_boundary(self):
        memory = self.machine()
        memory.write(0x00400FFE, b"\x01\x02\x03\x04")
        self.assertEqual(memory.read(0x00400FFE, 4), b"\x01\x02\x03\x04")

    def test_access_may_span_two_adjacent_regions(self):
        memory = self.machine()
        memory.write(0x00401FFE, b"\xAA\xBB\xCC\xDD")
        self.assertEqual(memory.read(0x00401FFE, 4), b"\xAA\xBB\xCC\xDD")

    def test_unmapped_address_is_a_bus_error_naming_it(self):
        memory = self.machine()
        with self.assertRaisesRegex(BusError, "0x20000000"):
            memory.read(0x20000000, 4)
        with self.assertRaises(BusError):
            memory.write(0x20000000, b"\x00")

    def test_access_running_off_the_end_of_memory_is_a_bus_error(self):
        memory = self.machine()
        with self.assertRaisesRegex(BusError, "0x00403000"):
            memory.read(0x00402FFC, 8)

    def test_peripheral_window_is_not_dma_memory(self):
        memory = self.machine()
        with self.assertRaises(BusError):
            memory.read(0x00802100, 4)

    def test_demand_range_is_readable_through_the_module(self):
        module = DemandModule("W", {0x1000: bytes([7]) * 0x1000})
        memory = self.machine(module)
        self.assertEqual(memory.read(0x1004, 4), b"\x07" * 4)

    def test_bad_block_of_a_demand_range_is_a_bus_error(self):
        module = DemandModule("W", {}, bad=[0x1020])
        memory = self.machine(module)
        with self.assertRaises(BusError):
            memory.read(0x1020, 4)
        self.assertEqual(memory.read(0x1000, 4), b"\x00" * 4)

    def test_demand_range_is_read_only(self):
        module = DemandModule("W", {})
        memory = self.machine(module)
        with self.assertRaises(BusError):
            memory.write(0x1000, b"\x01")


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

    def test_invalidate_with_a_range_inside_a_page_drops_that_page(self):
        module = DemandModule("W", {0x1000: bytes([7]) * 0x1000})
        cpu, _ = make_demand_machine(module)
        run_code(cpu, ldr_r1(cpu, 0x1000), LDR_R0_R1)
        module.pages[0x1000] = bytes([9]) * 0x1000
        module.invalidate("win", 0x1010, 8)
        run_code(cpu, ldr_r1(cpu, 0x1000), LDR_R0_R1)
        self.assertEqual(cpu.reg("r0"), 0x09090909)

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
