"""GPIO: 40 pins in two banks. Register layout: src/platform/soc/bk7221u/include/soc/gpio.h.

Each pin has a config word. A pin drives its pad when the output driver is on
(`output_enable` clear) and no peripheral function owns it; otherwise the pad
sits at the level the outside world forces with `set_input()`, or at its pull
resistor, or reads 0. Peripheral functions are not modelled.

Interrupt modes per pin (2 bits in intlv0/1/3), as the vendor driver uses them:
0 level low, 1 level high, 2 rising edge, 3 falling edge. A pin is pending while
it is enabled and latched; level modes latch again as long as the level holds.

Host side: `set_input(pin, level)` forces a pad (None releases it),
`pad_level(pin)` reads it, `watch(fn)` reports every pad change as fn(pin, level).
"""

from core.module import Module

PINS = 40
_BANK1_BASE = 0xC0
_SIZE = 0x100

_INPUT, _OUTPUT = 1 << 0, 1 << 1
_OUTPUT_DISABLE, _PULL_MODE, _PULL_ENABLE, _FUNCTION_ENABLE = 1 << 3, 1 << 4, 1 << 5, 1 << 6
_CFG_WRITABLE = 0x7E  # bit 0 is the pad level, bit 7 is not investigated

_INTEN, _INTLV0, _INTLV1, _INTSTA = 0x84, 0x88, 0x8C, 0x90
_EXTRA_INT_CFG, _DETECT = 0x98, 0x9C
_INTEN2, _INTLV3, _INTSTA2 = 0xE0, 0xE4, 0xE8

_LOW, _HIGH, _RISING, _FALLING = 0, 1, 2, 3


def _pin_offset(pin):
    return pin * 4 if pin < 32 else _BANK1_BASE + (pin - 32) * 4


def _pin_at(offset):
    if offset < 32 * 4:
        return offset // 4
    if _BANK1_BASE <= offset < _BANK1_BASE + 8 * 4:
        return 32 + (offset - _BANK1_BASE) // 4
    return None


class GpioModule(Module):
    def __init__(self, name, irq=None, pin_reset=0, reset_words=None, **params):
        super().__init__(name, **params)
        self._irq = irq
        self._pin_reset = pin_reset
        self._reset_words = {int(k, 0): v for k, v in (reset_words or {}).items()}
        self._external = [None] * PINS  # what the outside world forces; survives a chip reset
        self._watchers = []
        self.reset()

    def reset(self):
        self._regs = {_pin_offset(p): self._pin_reset for p in range(PINS)}
        self._regs.update(self._reset_words)
        self._latched = 0
        self._pad = [self._compute_pad(p) for p in range(PINS)]
        self._update_irq()

    def ranges(self):
        return [("regs", _SIZE)]

    def watch(self, callback):
        self._watchers.append(callback)

    def pad_level(self, pin):
        self._check_pin(pin)
        return self._pad[pin]

    def set_input(self, pin, level):
        self._check_pin(pin)
        self._external[pin] = None if level is None else bool(level)
        self._refresh(pin)

    def read(self, range_name, offset, size):
        pin = _pin_at(offset)
        if pin is not None:
            return (self._regs[offset] & _CFG_WRITABLE) | (_INPUT if self._pad[pin] else 0)
        if offset == _INTSTA:
            return self._pending() & 0xFFFFFFFF
        if offset == _INTSTA2:
            return self._pending() >> 32
        return self._regs.get(offset, 0)

    def write(self, range_name, offset, size, value):
        value &= 0xFFFFFFFF
        pin = _pin_at(offset)
        if pin is not None:
            self._regs[offset] = value & _CFG_WRITABLE
            self._refresh(pin)
        elif offset == _INTSTA:
            self._latched &= ~value
            self._settle()
        elif offset == _INTSTA2:
            self._latched &= ~(value << 32)
            self._settle()
        elif offset == _DETECT:
            pass
        elif offset == _EXTRA_INT_CFG:
            self._regs[offset] = value & 0x33  # the W1C status bits have no source
        else:
            self._regs[offset] = value
            if offset in (_INTEN, _INTEN2, _INTLV0, _INTLV1, _INTLV3):
                self._settle()

    @staticmethod
    def _check_pin(pin):
        if not 0 <= pin < PINS:
            raise ValueError("GPIO pin %d out of range (0-%d)" % (pin, PINS - 1))

    def _compute_pad(self, pin):
        cfg = self._regs[_pin_offset(pin)]
        if not cfg & _OUTPUT_DISABLE and not cfg & _FUNCTION_ENABLE:
            return bool(cfg & _OUTPUT)
        if self._external[pin] is not None:
            return self._external[pin]
        if cfg & _PULL_ENABLE:
            return bool(cfg & _PULL_MODE)
        return False

    def _refresh(self, pin):
        new = self._compute_pad(pin)
        old = self._pad[pin]
        if new != old:
            self._pad[pin] = new
            if self._enabled(pin):
                mode = self._mode(pin)
                if (mode == _RISING and new) or (mode == _FALLING and not new):
                    self._latched |= 1 << pin
            for watcher in self._watchers:
                watcher(pin, new)
        self._settle()

    def _enabled(self, pin):
        word = self._regs.get(_INTEN if pin < 32 else _INTEN2, 0)
        return bool(word >> (pin if pin < 32 else pin - 32) & 1)

    def _mode(self, pin):
        if pin < 16:
            word, index = self._regs.get(_INTLV0, 0), pin
        elif pin < 32:
            word, index = self._regs.get(_INTLV1, 0), pin - 16
        else:
            word, index = self._regs.get(_INTLV3, 0), pin - 32
        return word >> (index * 2) & 3

    def _settle(self):
        """Level modes latch for as long as their level holds."""
        for pin in range(PINS):
            if self._enabled(pin):
                mode = self._mode(pin)
                if (mode == _LOW and not self._pad[pin]) or (mode == _HIGH and self._pad[pin]):
                    self._latched |= 1 << pin
        self._update_irq()

    def _pending(self):
        enabled = 0
        for pin in range(PINS):
            if self._enabled(pin):
                enabled |= 1 << pin
        return self._latched & enabled

    def _update_irq(self):
        if self._irq is not None:
            self._irq.set(bool(self._pending()))
