"""RF transceiver control (RC, BK7011), 0x01050000. Layout: soc/include/rwnx/rc.h. Registers only.

The transceiver bank at +0x80 is written over an SPI shim whose busy bit is a plain
register here, so a write completes at once.
"""

from modules.wifi.bank import RegisterBank


class Rc(RegisterBank):
    KEY = "rc"
    SIZE = 480
