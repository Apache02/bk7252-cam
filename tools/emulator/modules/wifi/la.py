"""Logic analyzer (LA), 0x10E00000. Layout: soc/include/rwnx/la.h. Registers only."""

from modules.wifi.bank import RegisterBank


class La(RegisterBank):
    KEY = "la"
    SIZE = 16 * 4
