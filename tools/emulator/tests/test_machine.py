import struct
import unittest

from core.machine import Machine
from core.module import Fault, Module
from core.profile import MemorySpec, ModuleSpec, Profile


class Dummy(Module):
    instances = []

    def __init__(self, name, **params):
        super().__init__(name, **params)
        Dummy.instances.append(self)

    def ranges(self):
        return [("regs", 0x100)]


class Controller(Module):
    def __init__(self, name, **params):
        super().__init__(name, **params)
        self.lines = []

    def line(self, source):
        self.lines.append(source)
        return ("line", source)


def profile(*modules, memory=None):
    return Profile(
        name="TEST",
        insns_per_us=100,
        peripheral_window=(0x00800000, 0x100000),
        memory=memory or [MemorySpec("RAM1", 0x00400000, 0x1000)],
        aliases={},
        modules=list(modules),
    )


class MachineTest(unittest.TestCase):
    def setUp(self):
        Dummy.instances.clear()

    def test_builds_modules_from_profile(self):
        p = profile(
            ModuleSpec("dummy", "A", {"regs": 0x00802100}),
            ModuleSpec("dummy", "B", {"regs": 0x00802200}),
        )
        machine = Machine(p, {"dummy": Dummy}, {})
        self.assertEqual(sorted(machine.modules), ["A", "B"])
        self.assertIsNot(machine.modules["A"], machine.modules["B"])

    def test_unknown_module_type_rejected(self):
        p = profile(ModuleSpec("nope", "A", {"regs": 0x00802100}))
        with self.assertRaisesRegex(ValueError, "nope.*dummy"):
            Machine(p, {"dummy": Dummy}, {})

    def test_module_receives_collaborators(self):
        p = profile(ModuleSpec("dummy", "A", {"regs": 0x00802100}, {"size": 7}))
        machine = Machine(p, {"dummy": Dummy}, {"A": "endpoint"})
        params = machine.modules["A"].params
        self.assertEqual(params["size"], 7)
        self.assertEqual(params["endpoint"], "endpoint")
        self.assertIs(params["clock"], machine.scheduler)
        self.assertIn("cpu_irq", params)

    def test_irq_param_resolved_through_named_controller(self):
        p = profile(
            ModuleSpec("controller", "ICU", {"regs": 0x00802000}),
            ModuleSpec("dummy", "A", {"regs": 0x00802100}, {"intc": "ICU", "irq": 5}),
        )
        machine = Machine(p, {"dummy": Dummy, "controller": Controller}, {})
        self.assertEqual(machine.modules["A"].params["irq"], ("line", 5))

    def test_controller_listed_after_user_rejected(self):
        p = profile(
            ModuleSpec("dummy", "A", {"regs": 0x00802100}, {"intc": "ICU", "irq": 5}),
            ModuleSpec("controller", "ICU", {"regs": 0x00802000}),
        )
        with self.assertRaisesRegex(ValueError, "ICU"):
            Machine(p, {"dummy": Dummy, "controller": Controller}, {})


class Resettable(Module):
    resets = 0

    def ranges(self):
        return [("regs", 0x100)]

    def reset(self):
        Resettable.resets += 1

    def signal(self, name):
        return ("signal", name)


class ResetTest(unittest.TestCase):
    def setUp(self):
        Resettable.resets = 0

    def test_signal_param_resolved_through_named_module(self):
        p = profile(
            ModuleSpec("res", "ICU", {"regs": 0x00802000}),
            ModuleSpec("dummy", "A", {"regs": 0x00802100}, {"clock_off_signal": "ICU.gate"}),
        )
        machine = Machine(p, {"dummy": Dummy, "res": Resettable}, {})
        params = machine.modules["A"].params
        self.assertEqual(params["clock_off"], ("signal", "gate"))
        self.assertNotIn("clock_off_signal", params)

    def test_signal_module_listed_after_user_rejected(self):
        p = profile(
            ModuleSpec("dummy", "A", {"regs": 0x00802100}, {"clock_off_signal": "ICU.gate"}),
            ModuleSpec("res", "ICU", {"regs": 0x00802000}),
        )
        with self.assertRaisesRegex(ValueError, "ICU"):
            Machine(p, {"dummy": Dummy, "res": Resettable}, {})

    def test_reset_resets_modules_and_restarts_the_cpu(self):
        p = profile(ModuleSpec("res", "R", {"regs": 0x00802000}))
        machine = Machine(p, {"res": Resettable}, {})
        machine.cpu.reset(0x00400010)
        machine.reset()
        self.assertEqual(Resettable.resets, 1)
        self.assertEqual(machine.cpu.reg("pc"), 0)

    def test_reset_fills_each_region_with_its_own_byte(self):
        p = profile(memory=[
            MemorySpec("RAM1", 0x00400000, 0x1000, reset_fill=0xAA),
            MemorySpec("RAM2", 0x00900000, 0x1000, reset_fill=0x55),
        ])
        machine = Machine(p, {}, {})
        machine.cpu.mem_write(0x00400010, b"\x11\x22")
        machine.cpu.mem_write(0x00900010, b"\x33")
        machine.reset()
        self.assertEqual(machine.cpu.mem_read(0x00400010, 2), b"\xAA\xAA")
        self.assertEqual(machine.cpu.mem_read(0x00400FFF, 1), b"\xAA")  # last byte of the region
        self.assertEqual(machine.cpu.mem_read(0x00900010, 1), b"\x55")

    def test_region_without_reset_fill_keeps_its_contents(self):
        p = profile(memory=[
            MemorySpec("RAM1", 0x00400000, 0x1000, reset_fill=0xAA),
            MemorySpec("KEEP", 0x00900000, 0x1000),
        ])
        machine = Machine(p, {}, {})
        machine.cpu.mem_write(0x00900010, b"\x33")
        machine.reset()
        self.assertEqual(machine.cpu.mem_read(0x00900010, 1), b"\x33")

    def test_modules_can_reset_the_chip_through_their_reset_chip_param(self):
        p = profile(ModuleSpec("res", "R", {"regs": 0x00802000}))
        machine = Machine(p, {"res": Resettable}, {})
        machine.modules["R"].params["reset_chip"]()
        self.assertEqual(Resettable.resets, 1)


class FaultOnRead(Module):
    def ranges(self):
        return [("regs", 0x100)]

    def read(self, range_name, offset, size):
        raise Fault("dabt", 0x00802100)


class FaultWiringTest(unittest.TestCase):
    def test_module_fault_enters_data_abort(self):
        p = profile(ModuleSpec("faulty", "F", {"regs": 0x00802100}))
        machine = Machine(p, {"faulty": FaultOnRead}, {})
        cpu = machine.cpu
        cpu.mem_write(0x00400100, struct.pack("<I", 0x00802100))
        cpu.mem_write(0x00400000, struct.pack("<2I", 0xE59F10F8, 0xE5910000))
        cpu.reset(0x00400000)
        cpu.run(2)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x17)
        self.assertEqual(cpu.reg("lr"), 0x00400004 + 8)


if __name__ == "__main__":
    unittest.main()
