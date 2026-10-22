"""MAC core, 0xC0000000. Layout: soc/include/rwnx/mac_core.h.

Registers read back what was written, except the monotonic counters:
- counter 1 (0x11C): 32-bit, runs at 40 MHz;
- counter 2 (0x120 lo, 0x124 hi): a 48-bit microsecond counter, `hi` carries the upper 16
  bits and a self-clearing software-update bit [31]. Writing `hi` with it set loads the
  counter from the value last written to `lo` and the new `hi` (assumed from the header's
  name for the bit).

All of them read 0 while the MAC is powered down and start from 0 when it powers up
(`mac_powered` signal, a field of the system-control power register).
"""

from modules.wifi.bank import RegisterBank

_COUNTER_1, _COUNTER_2_LO, _COUNTER_2_HI = 0x11C, 0x120, 0x124
_SW_UPDATE = 1 << 31
_MASK_32, _MASK_48 = 0xFFFFFFFF, (1 << 48) - 1


class MacCore(RegisterBank):
    KEY = "mac_core"
    SIZE = 1376 * 4

    def __init__(self, name, clock=None, mac_powered=None, **params):
        self._clock = clock
        self._power = mac_powered
        super().__init__(name, **params)

    def reset(self):
        super().reset()
        self._start = None if self._power is not None else 0
        self._offset_us = 0
        self._lo_written = 0

    def tick(self, now_ns):
        self._elapsed_ns()

    def load(self, offset, size):
        word = offset & ~3
        if word not in (_COUNTER_1, _COUNTER_2_LO, _COUNTER_2_HI):
            return super().load(offset, size)
        value = self._counter(word) >> ((offset & 3) * 8)
        return value & ((1 << (size * 8)) - 1)

    def store(self, offset, size, value):
        word = offset & ~3
        if word == _COUNTER_1:
            return  # read-only
        if word == _COUNTER_2_LO:
            self._lo_written = value & _MASK_32
        elif word == _COUNTER_2_HI:
            self._software_update(value)
        else:
            super().store(offset, size, value)

    def _elapsed_ns(self):
        """Nanoseconds since the MAC powered up, or None while it is powered down."""
        if self._power is not None and not self._power.active():
            self._start = None
            self._offset_us = 0
            return None
        if self._start is None:
            self._start = self._clock.now_ns()
        return self._clock.now_ns() - self._start

    def _counter(self, word):
        elapsed = self._elapsed_ns()
        if elapsed is None:
            return 0
        if word == _COUNTER_1:
            return elapsed * 40 // 1000 & _MASK_32
        us = (elapsed // 1000 + self._offset_us) & _MASK_48
        return us & _MASK_32 if word == _COUNTER_2_LO else us >> 32

    def _software_update(self, value):
        elapsed = self._elapsed_ns()
        if elapsed is not None and value & _SW_UPDATE:
            target = (value & 0xFFFF) << 32 | self._lo_written
            self._offset_us = target - elapsed // 1000
