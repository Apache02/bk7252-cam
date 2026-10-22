"""MAC platform (PL) block, 0xC0008000. Layout: soc/include/rwnx/mac_pl.h.

Registers read back what was written, except `mac_cntrl_2.soft_reset` (+0x50, bit 0):
it is self-clearing, and the firmware polls it until the reset is done. Here it clears at
the next tick, one slice of device time after the write. The reset itself does nothing yet.
"""

from modules.wifi.bank import RegisterBank

_MAC_CNTRL_2 = 0x50
_SOFT_RESET = 1


class MacPl(RegisterBank):
    KEY = "mac_pl"
    SIZE = 351 * 4

    def reset(self):
        super().reset()
        self._reset_pending = False

    def store(self, offset, size, value):
        super().store(offset, size, value)
        if offset <= _MAC_CNTRL_2 < offset + size and value >> (8 * (_MAC_CNTRL_2 - offset)) & _SOFT_RESET:
            self._reset_pending = True

    def tick(self, now_ns):
        if self._reset_pending:
            self._reset_pending = False
            word = int.from_bytes(self._memory[_MAC_CNTRL_2:_MAC_CNTRL_2 + 4], "little")
            self._memory[_MAC_CNTRL_2:_MAC_CNTRL_2 + 4] = (word & ~_SOFT_RESET).to_bytes(4, "little")
            self._log.debug("%s soft reset done", self.name)
