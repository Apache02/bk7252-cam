"""A WiFi block that, for now, is just registers: whatever is written reads back.

`--debug wifi` logs every read and write of every WiFi block; each block has its own
logger, `emulator.wifi.<key>`, so `--debug wifi.mac_core` picks one.
"""

import logging

from core.module import Module


class RegisterBank(Module):
    KEY = ""  # logger suffix, also the block's short name
    SIZE = 0  # bytes, sizeof(hw_*_t) of the header in src/platform/soc/include/rwnx

    def __init__(self, name, **params):
        super().__init__(name, **params)
        self._log = logging.getLogger("emulator.wifi." + self.KEY)
        self.reset()

    def reset(self):
        self._memory = bytearray(self.SIZE)

    def ranges(self):
        return [("regs", self.SIZE)]

    def read(self, range_name, offset, size):
        value = self.load(offset, size)
        if self._log.isEnabledFor(logging.DEBUG):
            self._log.debug("%s read +0x%04X (%d bytes) = 0x%0*X", self.name, offset, size, size * 2, value)
        return value

    def write(self, range_name, offset, size, value):
        value &= (1 << (size * 8)) - 1
        if self._log.isEnabledFor(logging.DEBUG):
            self._log.debug("%s write +0x%04X (%d bytes) = 0x%0*X", self.name, offset, size, size * 2, value)
        self.store(offset, size, value)

    def peek(self, offset):
        """The stored word at `offset`, unlogged."""
        return int.from_bytes(self._memory[offset:offset + 4], "little")

    def poke(self, offset, value):
        """Set the stored word at `offset`, unlogged."""
        self._memory[offset:offset + 4] = (value & 0xFFFFFFFF).to_bytes(4, "little")

    def load(self, offset, size):
        """What a read returns; blocks with live registers override this."""
        return int.from_bytes(self._memory[offset:offset + size], "little")

    def store(self, offset, size, value):
        """What a write does; blocks with live registers override this."""
        count = min(size, self.SIZE - offset)  # a write may straddle the end of the block
        if count > 0:
            self._memory[offset:offset + count] = value.to_bytes(size, "little")[:count]
