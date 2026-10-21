"""Registry of module types the profile can name."""

from modules.flash import FlashModule
from modules.intc import IntcModule
from modules.timer import TimerModule
from modules.uart import UartModule

REGISTRY = {
    "flash": FlashModule,
    "intc": IntcModule,
    "timer": TimerModule,
    "uart": UartModule,
}
