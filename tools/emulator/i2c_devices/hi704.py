"""HI704 VGA camera sensor: 256 registers, the chip id 0x96 at register 0x04."""

from i2c_devices.register_device import RegisterDevice


class Hi704(RegisterDevice):
    NAME = "hi704"
    DEFAULT_ADDRESS = 0x30
    READ_ONLY = {0x04: 0x96}
