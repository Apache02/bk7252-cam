"""System control block. Layout: src/platform/soc/bk7221u/include/soc/sctrl.h.

The chip and device ids come from the profile and are read-only. Every other
register just reads back what was written; reset values come from the profile
(`reset_words`), and `warm_reset_words` overrides them after a chip reset where
the measured warm state differs (docs/por_reset_state.md).
"""

from core.module import Module

_CHIP_ID = 0x00
_DEVICE_ID = 0x04
_SIZE = 0x200


def _offsets(words):
    return {int(offset, 0): value for offset, value in (words or {}).items()}


class SctrlModule(Module):
    def __init__(self, name, chip_id=0, device_id=0, reset_words=None, warm_reset_words=None, **params):
        super().__init__(name, **params)
        self._chip_id = chip_id
        self._device_id = device_id
        self._cold = _offsets(reset_words)
        self._warm = {**self._cold, **_offsets(warm_reset_words)}
        self._words = dict(self._cold)

    def reset(self):
        self._words = dict(self._warm)

    def ranges(self):
        return [("regs", _SIZE)]

    def read(self, range_name, offset, size):
        if offset == _CHIP_ID:
            return self._chip_id
        if offset == _DEVICE_ID:
            return self._device_id
        return self._words.get(offset, 0)

    def write(self, range_name, offset, size, value):
        if offset not in (_CHIP_ID, _DEVICE_ID):
            self._words[offset] = value & 0xFFFFFFFF
