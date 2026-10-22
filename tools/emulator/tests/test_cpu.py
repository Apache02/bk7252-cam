import struct
import unittest

from unicorn import UC_HOOK_MEM_READ_UNMAPPED, UcError

from core.cpu import Cpu, RunResult

RAM = 0x00400000
MODE_ABT = 0x17
MODE_SVC = 0x13
I_BIT = 0x80


def program(*words):
    return struct.pack("<%dI" % len(words), *words)


MOV_R0_5 = 0xE3A00005
ADD_R0_7 = 0xE2800007
B_SELF = 0xEAFFFFFE
WFI = 0xEE070F90


def make_cpu(*words, at=RAM):
    cpu = Cpu()
    cpu.map_ram(RAM, 0x1000)
    cpu.mem_write(at, program(*words))
    cpu.reset(at)
    return cpu


class CpuTest(unittest.TestCase):
    def test_runs_arm_program(self):
        cpu = make_cpu(MOV_R0_5, ADD_R0_7, B_SELF)
        self.assertEqual(cpu.run(3), (RunResult.BUDGET, 3))
        self.assertEqual(cpu.reg("r0"), 12)

    def test_wfi_returns_idle(self):
        cpu = make_cpu(WFI, B_SELF)
        result, _ = cpu.run(10)
        self.assertEqual(result, RunResult.IDLE)

    def test_data_abort_entry(self):
        cpu = make_cpu(B_SELF)
        old_cpsr = cpu.reg("cpsr")
        cpu.enter_exception("dabt", 0x00400010)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, MODE_ABT)
        self.assertTrue(cpu.reg("cpsr") & I_BIT)
        self.assertEqual(cpu.reg("lr"), 0x00400018)
        self.assertEqual(cpu.reg("pc"), 0x00000010)
        self.assertEqual(cpu.reg("spsr"), old_cpsr)

    def test_prefetch_abort_lr_plus_4(self):
        cpu = make_cpu(B_SELF)
        cpu.enter_exception("pabt", 0x00400010)
        self.assertEqual(cpu.reg("lr"), 0x00400014)
        self.assertEqual(cpu.reg("pc"), 0x0000000C)

    def test_irq_taken_when_enabled(self):
        cpu = make_cpu(B_SELF)
        cpu.map_ram(0, 0x1000)
        cpu.mem_write(0x18, program(B_SELF))
        cpu.set_reg("cpsr", MODE_SVC)  # I bit clear
        cpu.raise_irq(True)
        cpu.run(1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x12)
        self.assertEqual(cpu.reg("pc") & ~3, 0x18)

    def test_posted_fault_from_hook_enters_data_abort(self):
        cpu = make_cpu(0xE59F10F8, 0xE5910000, B_SELF)  # ldr r1,[lit]; ldr r0,[r1]
        cpu.mem_write(RAM + 0x100, struct.pack("<I", 0x01000000))
        cpu.map_ram(0, 0x1000)

        def unmapped(uc, access, addr, size, value, _):
            cpu.post_fault("dabt")
            return False

        cpu.unicorn.hook_add(UC_HOOK_MEM_READ_UNMAPPED, unmapped)
        cpu.run(3)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, MODE_ABT)
        self.assertEqual(cpu.reg("lr"), RAM + 4 + 8)
        self.assertEqual(cpu.reg("pc"), 0x10)

    def test_swi_enters_svc_vector_with_lr_after_the_instruction(self):
        cpu = make_cpu(0xEF000000, B_SELF)  # swi 0
        cpu.map_ram(0, 0x1000)
        cpu.mem_write(0x08, program(B_SELF))
        cpu.set_reg("cpsr", MODE_SVC | 0x10)  # make the old CPSR recognisable
        old = cpu.reg("cpsr")
        cpu.run(1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, MODE_SVC)
        self.assertTrue(cpu.reg("cpsr") & I_BIT)
        self.assertEqual(cpu.reg("lr"), RAM + 4)
        self.assertEqual(cpu.reg("pc"), 0x08)
        self.assertEqual(cpu.reg("spsr"), old)

    def swi_loop(self):
        """A task that yields forever: swi, then jump back; the SWI handler returns at once."""
        cpu = make_cpu(0xEF000000, 0xEAFFFFFD)  # swi 0 ; b RAM
        cpu.map_ram(0, 0x1000)
        cpu.mem_write(0x08, program(0xE1B0F00E))  # movs pc, lr
        cpu.mem_write(0x18, program(B_SELF))
        cpu.set_reg("cpsr", MODE_SVC)  # I bit clear
        return cpu

    def test_an_irq_is_taken_even_when_every_slice_ends_in_a_swi(self):
        cpu = self.swi_loop()
        cpu.run(1000)  # the loop is under way; the line rises later
        cpu.raise_irq(True)
        for _ in range(20):
            cpu.run(1000)
            if cpu.reg("cpsr") & 0x1F == 0x12:
                break
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x12)

    def test_a_slice_that_ends_in_an_exception_still_counts_as_a_full_slice(self):
        cpu = self.swi_loop()
        self.assertEqual(cpu.run(1000), (RunResult.BUDGET, 1000))

    def test_the_swi_loop_keeps_running_with_no_interrupt_pending(self):
        cpu = self.swi_loop()
        for _ in range(5):
            self.assertEqual(cpu.run(1000)[0], RunResult.BUDGET)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, MODE_SVC)

    def test_undefined_instruction_enters_und_vector(self):
        cpu = make_cpu(0xE7F000F0, B_SELF)  # permanently undefined
        cpu.map_ram(0, 0x1000)
        cpu.mem_write(0x04, program(B_SELF))
        cpu.run(1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, 0x1B)
        self.assertEqual(cpu.reg("lr"), RAM + 4)
        self.assertEqual(cpu.reg("pc"), 0x04)

    def test_thumb_swi_returns_to_next_halfword_and_enters_arm_state(self):
        cpu = Cpu()
        cpu.map_ram(RAM, 0x1000)
        cpu.map_ram(0, 0x1000)
        cpu.mem_write(RAM, struct.pack("<HH", 0xDF00, 0xE7FE))  # svc 0 ; b .
        cpu.mem_write(0x08, program(B_SELF))
        cpu.reset(RAM)
        cpu.set_reg("cpsr", MODE_SVC | 0x20)
        cpu.run(1)
        self.assertEqual(cpu.reg("lr"), RAM + 2)
        self.assertFalse(cpu.reg("cpsr") & 0x20)
        self.assertEqual(cpu.reg("pc"), 0x08)

    def test_unposted_unicorn_error_propagates(self):
        cpu = make_cpu(0xE59F10F8, 0xE5910000, B_SELF)
        cpu.mem_write(RAM + 0x100, struct.pack("<I", 0x01000000))
        with self.assertRaises(UcError):
            cpu.run(3)

    def test_reset_clears_general_registers_so_reboots_look_like_a_cold_start(self):
        cpu = make_cpu(MOV_R0_5, B_SELF)
        cpu.run(1)
        cpu.set_reg("r12", 7)
        cpu.set_reg("lr", 0x1234)
        cpu.reset(RAM)
        for name in ("r0", "r12", "sp", "lr"):
            self.assertEqual(cpu.reg(name), 0, name)

    def test_reset_drops_pending_interrupt_lines(self):
        cpu = make_cpu(B_SELF)
        cpu.raise_irq(True)
        cpu.raise_fiq(True)
        cpu.reset(RAM)
        cpu.set_reg("cpsr", MODE_SVC)  # unmasked
        cpu.run(1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, MODE_SVC)

    def test_instruction_is_the_word_at_pc_in_arm_state(self):
        cpu = make_cpu(MOV_R0_5, ADD_R0_7, B_SELF)
        cpu.run(1)
        self.assertEqual(cpu.instruction(), (RAM + 4, ADD_R0_7, 4))

    def test_instruction_is_the_halfword_at_pc_in_thumb_state(self):
        cpu = Cpu()
        cpu.map_ram(RAM, 0x1000)
        cpu.mem_write(RAM, struct.pack("<HH", 0x4770, 0xE7FE))
        cpu.reset(RAM)
        cpu.set_reg("pc", RAM + 2 | 1)  # Unicorn takes Thumb state from bit 0 of PC
        self.assertEqual(cpu.instruction(), (RAM + 2, 0xE7FE, 2))

    def test_irq_ignored_when_masked(self):
        cpu = make_cpu(B_SELF)
        cpu.set_reg("cpsr", MODE_SVC | I_BIT)
        cpu.raise_irq(True)
        cpu.run(1)
        self.assertEqual(cpu.reg("cpsr") & 0x1F, MODE_SVC)
        self.assertEqual(cpu.reg("pc"), RAM)


if __name__ == "__main__":
    unittest.main()
