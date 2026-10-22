"""Automatic gain control (AGC), 0x01002000. Layout: soc/include/rwnx/agc.h. Registers only."""

from modules.wifi.bank import RegisterBank


class Agc(RegisterBank):
    KEY = "agc"
    SIZE = 44 * 4
