"""UART. Register layout: src/platform/soc/bk7221u/include/soc/uart.h.

TX drains instantly to the host endpoint. RX bytes arrive from the endpoint on
tick() and wait in a 128-byte FIFO; the rest wait in the module.
"""

from core.module import Module

_CONFIG, _FIFO_CONFIG, _FIFO_STATUS, _FIFO_DATA, _IRQ_ENABLE, _IRQ_STATUS = (
    i * 4 for i in range(6)
)
_SIZE = 0x20
_FIFO_DEPTH = 128

_TX_NEED_WRITE = 1 << 0
_RX_NEED_READ = 1 << 1
_TX_STOP_END = 1 << 5

_TX_EMPTY = 1 << 17
_RX_FULL = 1 << 18
_RX_EMPTY = 1 << 19
_WR_READY = 1 << 20
_RD_READY = 1 << 21


class UartModule(Module):
    def __init__(self, name, endpoint=None, irq=None, **params):
        super().__init__(name, **params)
        self._endpoint = endpoint
        self._irq = irq
        self.reset()

    def reset(self):
        self._rx = bytearray()
        self._backlog = bytearray()
        self._latched = 0
        self._enable = 0
        self._fifo_config = 0
        self._words = {}
        self._update_irq()

    def ranges(self):
        return [("regs", _SIZE)]

    def attach(self, endpoint):
        self._endpoint = endpoint

    def tick(self, now_ns):
        if self._endpoint is not None:
            self._backlog += self._endpoint.read_nowait()
        self._refill()
        self._update_irq()

    def read(self, range_name, offset, size):
        if offset == _FIFO_STATUS:
            return self._fifo_status()
        if offset == _FIFO_DATA:
            return self._pop() << 8
        if offset == _IRQ_ENABLE:
            return self._enable
        if offset == _IRQ_STATUS:
            return self._pending()
        if offset == _FIFO_CONFIG:
            return self._fifo_config
        return self._words.get(offset, 0)

    def write(self, range_name, offset, size, value):
        if offset == _FIFO_DATA:
            if self._endpoint is not None:
                self._endpoint.write(bytes([value & 0xFF]))
            self._latched |= _TX_STOP_END
        elif offset == _IRQ_ENABLE:
            self._enable = value & 0xFF
        elif offset == _IRQ_STATUS:
            self._latched &= ~value
        elif offset == _FIFO_CONFIG:
            self._fifo_config = value
        elif offset != _FIFO_STATUS:
            self._words[offset] = value
        self._update_irq()

    def _pop(self):
        byte = self._rx.pop(0) if self._rx else 0
        self._refill()
        self._update_irq()
        return byte

    def _refill(self):
        room = _FIFO_DEPTH - len(self._rx)
        if room > 0 and self._backlog:
            self._rx += self._backlog[:room]
            del self._backlog[:room]

    def _fifo_status(self):
        count = len(self._rx)
        status = (count << 8) | _TX_EMPTY | _WR_READY
        status |= _RD_READY if count else _RX_EMPTY
        if count >= _FIFO_DEPTH:
            status |= _RX_FULL
        return status

    def _raw(self):
        raw = self._latched | _TX_NEED_WRITE
        threshold = (self._fifo_config >> 8) & 0xFF
        if self._rx and len(self._rx) >= threshold:
            raw |= _RX_NEED_READ
        return raw

    def _pending(self):
        return self._raw() & self._enable

    def _update_irq(self):
        if self._irq is not None:
            self._irq.set(bool(self._pending()))
