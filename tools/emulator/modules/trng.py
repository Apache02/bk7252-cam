"""True random number generator. Registers: docs/hardware/random.md.

Not random: the enabled generator returns a read counter (0, 1, 2, ...), so runs are
deterministic. It starts enabled, which lets the firmware skip its start-up delay.
While disabled `data` is a plain latch.
"""

from core.module import Module

_CTRL, _DATA = 0x00, 0x04


class TrngModule(Module):
    def __init__(self, name, **params):
        super().__init__(name, **params)
        self.reset()

    def reset(self):
        self._ctrl = 1
        self._count = 0
        self._latch = 0

    def ranges(self):
        return [("regs", 8)]

    def read(self, range_name, offset, size):
        if offset == _CTRL:
            return self._ctrl
        if offset == _DATA:
            if self._ctrl & 1:
                value = self._count
                self._count = (self._count + 1) & 0xFFFFFFFF
                return value
            return self._latch
        return 0

    def write(self, range_name, offset, size, value):
        if offset == _CTRL:
            self._ctrl = value & 1
        elif offset == _DATA and not self._ctrl & 1:
            self._latch = value & 0xFFFFFFFF
