"""One bank of three periodic timers. Register layout: soc/timer.h.

Each timer reloads from its counter register and counts a source clock divided
by (clk_divider + 1); when it expires it sets its irq_status bit (write 1 to
clear) and keeps running. The chip has no readable count register.
"""

from core.module import Module

_COUNTERS = (0x00, 0x04, 0x08)
_CTL = 0x0C
_SIZE = 0x10
_STATUS_SHIFT = 7
_NS = 1_000_000_000


class TimerModule(Module):
    def __init__(self, name, clock=None, irq=None, hz=26_000_000, **params):
        super().__init__(name, **params)
        self._clock = clock
        self._irq = irq
        self._hz = hz
        self.reset()

    def reset(self):
        self._counter = [0, 0, 0]
        self._enable = 0
        self._divider = 0
        self._status = 0
        self._next = [None, None, None]
        self._update_irq()

    def ranges(self):
        return [("regs", _SIZE)]

    def read(self, range_name, offset, size):
        if offset in _COUNTERS:
            return self._counter[offset // 4]
        if offset == _CTL:
            return self._enable | (self._divider << 3) | (self._status << _STATUS_SHIFT)
        return 0

    def write(self, range_name, offset, size, value):
        if offset in _COUNTERS:
            self._counter[offset // 4] = value & 0xFFFFFFFF
        elif offset == _CTL:
            self._write_ctl(value)

    def tick(self, now_ns):
        for n in range(3):
            due = self._next[n]
            if due is not None and due <= now_ns:
                self._status |= 1 << n
                period = self._period_ns(n)
                self._next[n] = due + ((now_ns - due) // period + 1) * period
        self._update_irq()

    def next_event_ns(self):
        pending = [t for t in self._next if t is not None]
        return min(pending) if pending else None

    def _write_ctl(self, value):
        now = self._clock.now_ns() if self._clock is not None else 0
        self._divider = (value >> 3) & 0xF
        self._status &= ~((value >> _STATUS_SHIFT) & 7)
        enable = value & 7
        for n in range(3):
            running = bool(enable & (1 << n)) and self._counter[n] > 0
            was_running = self._next[n] is not None
            if running and not was_running:
                self._next[n] = now + self._period_ns(n)
            elif not running:
                self._next[n] = None
        self._enable = enable
        self._update_irq()

    def _period_ns(self, n):
        ticks = max(self._counter[n], 1) * (self._divider + 1)
        return max(ticks * _NS // self._hz, 1)

    def _update_irq(self):
        if self._irq is not None:
            self._irq.set(bool(self._status))
