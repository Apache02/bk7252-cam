"""The WiFi blocks: MAC, modem, AGC, RF transceiver control and their helpers.

Profile type names are the keys of WIFI_MODULES.
"""

from modules.wifi.agc import Agc
from modules.wifi.la import La
from modules.wifi.mac_core import MacCore
from modules.wifi.mac_pl import MacPl
from modules.wifi.mac_pta import MacPta
from modules.wifi.mdm_cfg import MdmCfg
from modules.wifi.mdm_stat import MdmStat
from modules.wifi.nx_dma import NxDma
from modules.wifi.nx_intc import NxIntc
from modules.wifi.rc import Rc

WIFI_MODULES = {
    "wifi_agc": Agc,
    "wifi_la": La,
    "wifi_mac_core": MacCore,
    "wifi_mac_pl": MacPl,
    "wifi_mac_pta": MacPta,
    "wifi_mdm_cfg": MdmCfg,
    "wifi_mdm_stat": MdmStat,
    "wifi_nx_dma": NxDma,
    "wifi_nx_intc": NxIntc,
    "wifi_rc": Rc,
}
