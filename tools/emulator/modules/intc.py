"""Interrupt controller (ICU block): routes source bits to the core's IRQ and FIQ.

Register layout: src/platform/soc/bk7221u/include/soc/icu.h. Sources 0-15 raise
IRQ, 16-31 raise FIQ. A source stays pending until software writes 1 to it
and its cause is gone; while the cause persists it re-latches at once.
"""

from core.module import Module

_IRQ_ENABLE = 0x40
_GLOBAL_EN = 0x44
_RAW = 0x48
_STATUS = 0x4C
_SIZE = 0x54
_IRQ_MASK = 0x0000FFFF
_FIQ_MASK = 0xFFFF0000
_GLOBAL_IRQ, _GLOBAL_FIQ = 1, 2


class _Source:
    """One requester's line into a source bit; several may share a bit."""

    def __init__(self, intc, number):
        self._intc = intc
        self.number = number
        self.level = False

    def set(self, level):
        self.level = level
        self._intc.drive(self.number, level)


class IntcModule(Module):
    def __init__(self, name, cpu_irq=None, cpu_fiq=None, **params):
        super().__init__(name, **params)
        self._cpu_irq = cpu_irq
        self._cpu_fiq = cpu_fiq
        self._sources = []
        self._latched = 0
        self._enable = 0
        self._global = 0
        self._words = {}

    def ranges(self):
        return [("regs", _SIZE)]

    def line(self, source):
        line = _Source(self, source)
        self._sources.append(line)
        return line

    def drive(self, number, level):
        if level:
            self._latched |= 1 << number
        self._update()

    @property
    def _level(self):
        bits = 0
        for source in self._sources:
            if source.level:
                bits |= 1 << source.number
        return bits

    def read(self, range_name, offset, size):
        if offset == _IRQ_ENABLE:
            return self._enable
        if offset == _GLOBAL_EN:
            return self._global
        if offset == _RAW:
            return self._raw()
        if offset == _STATUS:
            return self._raw() & self._enable
        return self._words.get(offset, 0)

    def write(self, range_name, offset, size, value):
        if offset == _IRQ_ENABLE:
            self._enable = value & 0xFFFFFFFF
        elif offset == _GLOBAL_EN:
            self._global = value & 3
        elif offset in (_RAW, _STATUS):
            self._latched &= ~value
        else:
            self._words[offset] = value
        self._update()

    def _raw(self):
        return self._latched | self._level

    def _update(self):
        status = self._raw() & self._enable
        if self._cpu_irq is not None:
            self._cpu_irq.set(bool(self._global & _GLOBAL_IRQ and status & _IRQ_MASK))
        if self._cpu_fiq is not None:
            self._cpu_fiq.set(bool(self._global & _GLOBAL_FIQ and status & _FIQ_MASK))
