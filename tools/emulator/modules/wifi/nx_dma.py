"""NX DMA of the WiFi MAC, 0x10A00000. Layout: soc/include/rwnx/dma.h. Registers only."""

from modules.wifi.bank import RegisterBank


class NxDma(RegisterBank):
    KEY = "nx_dma"
    SIZE = 49 * 4
