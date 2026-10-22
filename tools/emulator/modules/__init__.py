"""Registry of module types the profile can name."""

from modules.flash import FlashModule
from modules.intc import IntcModule
from modules.timer import TimerModule
from modules.uart import UartModule
from modules.wdt import WdtModule

REGISTRY = {
    "flash": FlashModule,
    "intc": IntcModule,
    "timer": TimerModule,
    "uart": UartModule,
    "wdt": WdtModule,
}
