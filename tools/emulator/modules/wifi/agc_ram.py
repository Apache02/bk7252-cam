"""AGC microcode RAM, 0x0100A000 (AGC/MDM base + 0xA000). Plain memory."""

from modules.wifi.bank import RegisterBank


class AgcRam(RegisterBank):
    KEY = "agc_ram"
    SIZE = 512 * 4
