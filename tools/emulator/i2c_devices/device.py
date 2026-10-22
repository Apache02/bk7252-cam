"""What a device on an I2C bus looks like to the controller module."""


class I2cDevice:
    """A slave. The controller calls `begin` after the address byte, then `write` or `read`
    once per data byte, then `stop` when the bus transaction ends. `NAME` is the type name
    for the command line; `DEFAULT_ADDRESS` is the 7-bit address when none is given."""

    NAME = ""
    DEFAULT_ADDRESS = 0

    def __init__(self, address=None):
        self.address = self.DEFAULT_ADDRESS if address is None else address

    def begin(self, read):
        """The address byte matched this device; returns True to acknowledge it."""
        return True

    def write(self, byte):
        """A byte from the master; returns True to acknowledge it."""
        return True

    def read(self):
        """The next byte for the master."""
        return 0xFF

    def stop(self):
        """The transaction ended."""
