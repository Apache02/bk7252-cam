"""Modem configuration, 0x01000800. Layout: soc/include/rwnx/mdm_cfg.h. Registers only."""

from modules.wifi.bank import RegisterBank


class MdmCfg(RegisterBank):
    KEY = "mdm_cfg"
    SIZE = 38 * 4
