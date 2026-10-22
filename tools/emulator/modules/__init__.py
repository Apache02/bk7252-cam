"""Registry of module types the profile can name."""

from modules.flash import FlashModule
from modules.gdma import GdmaModule
from modules.gpio import GpioModule
from modules.intc import IntcModule
from modules.sctrl import SctrlModule
from modules.timer import TimerModule
from modules.uart import UartModule
from modules.wdt import WdtModule
from modules.wifi import WIFI_MODULES

REGISTRY = {
    **WIFI_MODULES,
    "flash": FlashModule,
    "gdma": GdmaModule,
    "gpio": GpioModule,
    "intc": IntcModule,
    "sctrl": SctrlModule,
    "timer": TimerModule,
    "uart": UartModule,
    "wdt": WdtModule,
}
