"""Devices that can sit on an emulated I2C bus, by the type name the command line uses."""

from i2c_devices.hi704 import Hi704

DEVICES = {device.NAME: device for device in (Hi704,)}

_USAGE = "want TYPE[@ADDRESS], ADDRESS a 7-bit number 0x08-0x77"


def parse_device_spec(text):
    """`TYPE` or `TYPE@ADDRESS` to a device object; ValueError with a message for the user."""
    name, at, address_text = text.partition("@")
    device = DEVICES.get(name.lower())
    if device is None:
        raise ValueError("unknown --i2c1 device %r (known: %s)" % (name, ", ".join(sorted(DEVICES))))
    if not at:
        return device()
    try:
        address = int(address_text, 0)
        if not 0x08 <= address <= 0x77:
            raise ValueError
    except ValueError:
        raise ValueError("bad --i2c1 %r (%s)" % (text, _USAGE)) from None
    return device(address)
