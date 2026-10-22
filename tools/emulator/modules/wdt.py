"""Watchdog. Behaviour: docs/hardware/wdt.md.

One register: period in ms [15:0] and a reload key [23:16] (0x5A, then 0xA5). The
countdown runs only while its clock is powered (`clock_off` signal low) and expiry
resets the whole chip.
"""

import logging

from core.module import Module

_KEY1, _KEY2 = 0x5A, 0xA5
_DEFAULT = 0x4000
_MS = 1_000_000

log = logging.getLogger("emulator.wdt")


class WdtModule(Module):
    def __init__(self, name, clock=None, clock_off=None, reset_chip=None, **params):
        super().__init__(name, **params)
        self._clock = clock
        self._clock_off = clock_off
        self._reset_chip = reset_chip
        self.reset()

    def reset(self):
        self._ctrl = _DEFAULT
        self._start = None
        self._key1_seen = False

    def ranges(self):
        return [("regs", 4)]

    def read(self, range_name, offset, size):
        return self._ctrl

    def write(self, range_name, offset, size, value):
        self._ctrl = value & 0xFFFFFF
        key = (value >> 16) & 0xFF
        if key == _KEY1:
            self._key1_seen = True
        elif key == _KEY2 and self._key1_seen:
            self._key1_seen = False
            self._start = self._clock.now_ns() if self._running() else None
        else:
            self._key1_seen = False

    def tick(self, now_ns):
        if not self._running():
            self._start = None
            return
        if self._start is None:
            self._start = now_ns
        if now_ns >= self._deadline():
            log.warning("watchdog expired: resetting the chip")
            self._start = None
            if self._reset_chip is not None:
                self._reset_chip()

    def next_event_ns(self):
        if self._start is None or not self._running():
            return None
        return self._deadline()

    def _running(self):
        return self._clock_off is None or not self._clock_off.active()

    def _deadline(self):
        return self._start + (self._ctrl & 0xFFFF) * _MS
