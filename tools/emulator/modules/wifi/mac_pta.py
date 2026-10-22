"""MAC packet traffic arbitration (PTA), 0xC0010000. Layout: soc/include/rwnx/mac_pta.h. Registers only."""

from modules.wifi.bank import RegisterBank


class MacPta(RegisterBank):
    KEY = "mac_pta"
    SIZE = 10 * 4
