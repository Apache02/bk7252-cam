"""A device that looks like 256 bytes of memory on the bus."""

from i2c_devices.device import I2cDevice


class RegisterDevice(I2cDevice):
    """The first byte of a write sets the offset; every further byte is stored there and the
    offset moves on. A read returns the byte at the offset and moves it on. The offset wraps at
    256 and survives a STOP, so a write of the offset can be followed by a separate read.

    `READ_ONLY` maps an offset to a fixed value: that register reads the value and ignores
    writes."""

    READ_ONLY = {}

    def __init__(self, address=None):
        super().__init__(address)
        self._memory = bytearray(256)
        for offset, value in self.READ_ONLY.items():
            self._memory[offset] = value
        self._offset = 0
        self._expect_offset = False

    def begin(self, read):
        self._expect_offset = not read
        return True

    def write(self, byte):
        if self._expect_offset:
            self._offset = byte
            self._expect_offset = False
        else:
            if self._offset not in self.READ_ONLY:
                self._memory[self._offset] = byte
            self._offset = (self._offset + 1) & 0xFF
        return True

    def read(self):
        value = self._memory[self._offset]
        self._offset = (self._offset + 1) & 0xFF
        return value
