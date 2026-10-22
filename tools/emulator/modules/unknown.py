"""A block of registers nobody has described yet. Plain memory that tells you who uses it.

Every read and write is an INFO line with the offset, the value and the address and opcode
of the instruction that made the access. `reset_words` ({"0x10": value}, as in the other
modules) gives registers a start value.
"""

import logging

from core.module import Module

log = logging.getLogger("emulator.unknown")


class UnknownModule(Module):
    def __init__(self, name, size=0x100, reset_words=None, cpu_instruction=None, **params):
        super().__init__(name, **params)
        self._size = size
        self._reset_words = {int(k, 0): v for k, v in (reset_words or {}).items()}
        self._instruction = cpu_instruction
        self.reset()

    def reset(self):
        self._memory = bytearray(self._size)
        for offset, word in self._reset_words.items():
            self._memory[offset:offset + 4] = (word & 0xFFFFFFFF).to_bytes(4, "little")

    def ranges(self):
        return [("regs", self._size)]

    def read(self, range_name, offset, size):
        value = int.from_bytes(self._memory[offset:offset + size], "little")
        self._report("read", offset, size, value)
        return value

    def write(self, range_name, offset, size, value):
        value &= (1 << (size * 8)) - 1
        self._report("write", offset, size, value)
        count = min(size, self._size - offset)  # a write may straddle the end of the block
        if count > 0:
            self._memory[offset:offset + count] = value.to_bytes(size, "little")[:count]

    def _report(self, what, offset, size, value):
        where = ""
        if self._instruction is not None:
            pc, opcode, width = self._instruction()
            where = "  pc 0x%08X insn %0*X" % (pc, width * 2, opcode)
        log.info("%s %s +0x%04X (%d bytes) = 0x%0*X%s", self.name, what, offset, size, size * 2, value, where)
