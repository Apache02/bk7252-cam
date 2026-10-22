"""Unicorn wrapper for the ARM core.

Spike results (Unicorn 2.1.4):
- ARM968E-S is not a model; UC_CPU_ARM_946 is the closest (ARMv5TE, no MMU needed).
- `mcr p15, 0, rX, c7, c0, 4` (wait for interrupt) halts emu_start() with PC
  just past the instruction, so idle is recognised by the opcode before PC.
- Exception and IRQ/FIQ entry are not modelled by Unicorn for a bare core with
  no vector memory handling we want, so they are done here by hand.
"""

import enum
import struct

from unicorn import (
    UC_ARCH_ARM,
    UC_ERR_INSN_INVALID,
    UC_HOOK_INTR,
    UC_MODE_ARM,
    UC_PROT_ALL,
    UC_PROT_READ,
    UC_PROT_WRITE,
    Uc,
    UcError,
)
from unicorn import arm_const as arm

_REGS = {
    **{"r%d" % i: getattr(arm, "UC_ARM_REG_R%d" % i) for i in range(13)},
    "sp": arm.UC_ARM_REG_SP,
    "lr": arm.UC_ARM_REG_LR,
    "pc": arm.UC_ARM_REG_PC,
    "cpsr": arm.UC_ARM_REG_CPSR,
    "spsr": arm.UC_ARM_REG_SPSR,
}

MODE_FIQ, MODE_IRQ, MODE_SVC, MODE_ABT, MODE_UND = 0x11, 0x12, 0x13, 0x17, 0x1B
I_BIT, F_BIT, T_BIT = 0x80, 0x40, 0x20

# kind -> (mode, vector offset, LR offset from the aborting instruction)
_EXCEPTIONS = {
    "undef": (MODE_UND, 0x04, 4),
    "swi": (MODE_SVC, 0x08, 4),
    "pabt": (MODE_ABT, 0x0C, 4),
    "dabt": (MODE_ABT, 0x10, 8),
}

_WFI_ARM = 0xEE070F90
_WFI_THUMB = (0xEE07, 0x0F90)
_NEVER = 0xFFFFFFFF
_INTNO_SWI = 2


class RunResult(enum.Enum):
    BUDGET = "budget"
    IDLE = "idle"
    STOPPED = "stopped"


class Cpu:
    def __init__(self):
        self._uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self._uc.ctl_set_cpu_model(arm.UC_CPU_ARM_946)
        self._irq = False
        self._fiq = False
        self._stop = False
        self._fault = None
        self._restart = False
        self._bad_interrupt = None
        self._uc.hook_add(UC_HOOK_INTR, self._on_interrupt)

    @property
    def unicorn(self):
        return self._uc

    def map_ram(self, base, size, executable=True):
        self._uc.mem_map(base, size, UC_PROT_ALL if executable else UC_PROT_READ | UC_PROT_WRITE)

    def mem_write(self, addr, data):
        self._uc.mem_write(addr, bytes(data))

    def mem_read(self, addr, size):
        return bytes(self._uc.mem_read(addr, size))

    def reg(self, name):
        return self._uc.reg_read(_REGS[name])

    def set_reg(self, name, value):
        self._uc.reg_write(_REGS[name], value)

    def reset(self, pc=0):
        self.set_reg("cpsr", MODE_SVC | I_BIT | F_BIT)
        for name in [*("r%d" % i for i in range(13)), "sp", "lr"]:
            self.set_reg(name, 0)
        self.set_reg("pc", pc)
        self._irq = self._fiq = False
        self._fault = None
        self._restart = False

    def raise_irq(self, on):
        self._irq = on

    def raise_fiq(self, on):
        self._fiq = on

    def post_fault(self, kind):
        """Called from a memory callback: stop, then raise the exception at the
        instruction being executed (PC is exact inside Unicorn memory hooks)."""
        if self._fault is None:
            self._fault = (kind, self.reg("pc"))
        self._uc.emu_stop()

    def request_restart(self):
        """From a callback: stop and re-execute the current instruction afresh."""
        self._restart = True
        self._uc.emu_stop()

    def stop(self):
        self._stop = True
        self._uc.emu_stop()

    def enter_exception(self, kind, fault_pc):
        mode, vector, lr_offset = _EXCEPTIONS[kind]
        if kind in ("undef", "swi") and self.reg("cpsr") & T_BIT:
            lr_offset = 2
        self._enter(mode, vector, fault_pc + lr_offset, I_BIT)

    def _on_interrupt(self, uc, intno, _):
        """Unicorn reports SWI here with PC already past the instruction."""
        if intno == _INTNO_SWI:
            width = 2 if self.reg("cpsr") & T_BIT else 4
            self._fault = ("swi", self.reg("pc") - width)
        else:
            self._bad_interrupt = intno
        self._uc.emu_stop()

    def run(self, insn_budget):
        if self._stop:
            return RunResult.STOPPED, 0
        self._take_pending()
        while True:
            cpsr = self.reg("cpsr")
            start = self.reg("pc") | (1 if cpsr & T_BIT else 0)
            try:
                self._uc.emu_start(start, _NEVER, count=insn_budget)
            except UcError as err:
                if err.errno == UC_ERR_INSN_INVALID and self._fault is None:
                    self._fault = ("undef", self.reg("pc"))
                elif self._fault is None:
                    raise
            if self._bad_interrupt is not None:
                raise RuntimeError("unhandled CPU exception %d" % self._bad_interrupt)
            if not self._restart or self._fault is not None or self._stop:
                break
            self._restart = False
            # Cached blocks were translated without the new hooks.
            self._uc.ctl_flush_tb()
        if self._stop:
            return RunResult.STOPPED, 0
        if self._fault is not None:
            kind, fault_pc = self._fault
            self._fault = None
            self.enter_exception(kind, fault_pc)
            return RunResult.BUDGET, 1
        if self._at_wfi():
            return RunResult.IDLE, 1
        return RunResult.BUDGET, insn_budget

    def _enter(self, mode, vector, lr, mask):
        old = self.reg("cpsr")
        self.set_reg("cpsr", (old & ~(0x3F)) | mode | mask)
        self.set_reg("spsr", old)
        self.set_reg("lr", lr)
        self.set_reg("pc", vector)

    def _take_pending(self):
        cpsr = self.reg("cpsr")
        pc = self.reg("pc")
        if self._fiq and not cpsr & F_BIT:
            self._enter(MODE_FIQ, 0x1C, pc + 4, I_BIT | F_BIT)
        elif self._irq and not cpsr & I_BIT:
            self._enter(MODE_IRQ, 0x18, pc + 4, I_BIT)

    def _at_wfi(self):
        pc = self.reg("pc") & ~1
        try:
            word = self.mem_read(pc - 4, 4)
        except Exception:
            return False
        if self.reg("cpsr") & T_BIT:
            return struct.unpack("<HH", word) == _WFI_THUMB
        return struct.unpack("<I", word)[0] == _WFI_ARM
