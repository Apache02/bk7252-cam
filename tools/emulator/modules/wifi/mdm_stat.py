"""Modem status, 0x01000000. Layout: soc/include/rwnx/mdm_stat.h. Registers only."""

from modules.wifi.bank import RegisterBank


class MdmStat(RegisterBank):
    KEY = "mdm_stat"
    SIZE = 27 * 4
