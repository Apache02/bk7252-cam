"""Registry of module types the profile can name."""

from modules.flash import FlashModule
from modules.gdma import GdmaModule
from modules.gpio import GpioModule
from modules.i2c1 import I2c1Module
from modules.intc import IntcModule
from modules.jpeg import JpegModule
from modules.sctrl import SctrlModule
from modules.timer import TimerModule
from modules.trng import TrngModule
from modules.uart import UartModule
from modules.unknown import UnknownModule
from modules.wdt import WdtModule
from modules.wifi import WIFI_MODULES

REGISTRY = {
    **WIFI_MODULES,
    "flash": FlashModule,
    "gdma": GdmaModule,
    "gpio": GpioModule,
    "i2c1": I2c1Module,
    "intc": IntcModule,
    "jpeg": JpegModule,
    "sctrl": SctrlModule,
    "timer": TimerModule,
    "trng": TrngModule,
    "uart": UartModule,
    "unknown": UnknownModule,
    "wdt": WdtModule,
}
