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
        self._loaded_ms = _DEFAULT  # period the running countdown was started with
        self._kicked = False
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
            self._kick()
        else:
            self._key1_seen = False

    def _kick(self):
        now = self._clock.now_ns()
        period = self._ctrl & 0xFFFF
        if not self._running():
            self._start = None
            log.debug("kick, period %d ms: the clock is powered down, nothing counts", period)
            return
        label = "kick" if self._kicked else "first kick"
        if self._start is None:
            log.debug("%s, period %d ms: the countdown had not started", label, period)
        else:
            elapsed = now - self._start
            percent = round(elapsed * 100 / (self._loaded_ms * _MS)) if self._loaded_ms else 0
            postponed = now + period * _MS - (self._start + self._loaded_ms * _MS)
            log.debug(
                "%s, period %d ms: %.1f ms since the last kick (%d%% of %d ms), "
                "reset postponed by %.1f ms",
                label, period, elapsed / _MS, percent, self._loaded_ms, postponed / _MS,
            )
        self._start = now
        self._loaded_ms = period
        self._kicked = True

    def tick(self, now_ns):
        if not self._running():
            self._start = None
            return
        if self._start is None:
            self._start = now_ns
            self._loaded_ms = self._ctrl & 0xFFFF
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
