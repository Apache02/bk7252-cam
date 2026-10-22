"""NX interrupt controller of the WiFi MAC, 0x10910000. Layout: soc/include/rwnx/intc.h. Registers only."""

from modules.wifi.bank import RegisterBank


class NxIntc(RegisterBank):
    KEY = "nx_intc"
    SIZE = 17 * 4
