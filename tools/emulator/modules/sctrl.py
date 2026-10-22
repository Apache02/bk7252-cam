"""System control block. Layout: src/platform/soc/bk7221u/include/soc/sctrl.h.

The chip and device ids come from the profile and are read-only. Every other
register just reads back what was written; reset values come from the profile
(`reset_words`), and `warm_reset_words` overrides them after a chip reset where
the measured warm state differs (docs/por_reset_state.md).
"""

from core.module import Module
from core.signals import make_signals

_CHIP_ID = 0x00
_DEVICE_ID = 0x04
_SIZE = 0x200


def _offsets(words):
    return {int(offset, 0): value for offset, value in (words or {}).items()}


class SctrlModule(Module):
    """`read_adjust` is a list of {offset, shift, width, add}: reads of that register show the
    field `add` higher than what is stored (stopping at the field's maximum). The firmware's
    own writes are stored as they are."""

    def __init__(
        self, name, chip_id=0, device_id=0, reset_words=None, warm_reset_words=None, signals=None,
        read_adjust=None, **params
    ):
        super().__init__(name, **params)
        self._chip_id = chip_id
        self._device_id = device_id
        self._read_adjust = read_adjust or []
        self._signals = make_signals(lambda offset: self._words.get(offset, 0), signals or {})
        self._cold = _offsets(reset_words)
        self._warm = {**self._cold, **_offsets(warm_reset_words)}
        self._words = dict(self._cold)

    def reset(self):
        self._words = dict(self._warm)

    def signal(self, name):
        return self._signals[name]

    def ranges(self):
        return [("regs", _SIZE)]

    def read(self, range_name, offset, size):
        if offset == _CHIP_ID:
            return self._chip_id
        if offset == _DEVICE_ID:
            return self._device_id
        word = self._words.get(offset, 0)
        for adjust in self._read_adjust:
            if adjust["offset"] == offset:
                mask = (1 << adjust["width"]) - 1
                field = word >> adjust["shift"] & mask
                word = word & ~(mask << adjust["shift"]) | min(field + adjust["add"], mask) << adjust["shift"]
        return word

    def write(self, range_name, offset, size, value):
        if offset not in (_CHIP_ID, _DEVICE_ID):
            self._words[offset] = value & 0xFFFFFFFF
