"""SAR ADC. Register layout: src/platform/soc/bk7221u/include/soc/saradc.h.

While `config.chnl_en` is set, one sample of `sample` arrives every 100 us (device time) into
a 32-sample FIFO; there is no analog input and the conversion timing fields are ignored.
Reading `data` or `dat_after_sta` pops a sample (the driver's ISR drains the FIFO through
`dat_after_sta`). A sample raises the interrupt once `ctrl_cfg.almost_cfg` samples wait
(at least one); the flag stays until `config.int_clr` is written, which reads back 0.
Clearing `chnl_en` stops the conversion and empties the FIFO.
"""

from core.module import Module

_CONFIG, _DATA, _CTRL_CFG, _SATURATION_CFG, _DAT_AFTER_STA = (i * 4 for i in range(5))
_SIZE = 0x20
_CHNL_EN, _INT_CLR = 1 << 2, 1 << 8
_BUSY, _FIFO_EMPTY, _FIFO_FULL = 1 << 29, 1 << 30, 1 << 31
_STATUS = _BUSY | _FIFO_EMPTY | _FIFO_FULL
_DEPTH = 32
_SAMPLE_NS = 100_000
_CTRL_CFG_RESET = 0xE0
_DAT_AFTER_STA_EMPTY = 0xFFFF


class SaradcModule(Module):
    def __init__(self, name, irq=None, sample=330, **params):
        super().__init__(name, **params)
        self._irq = irq
        self._sample = sample
        self.reset()

    def reset(self):
        self._config = 0
        self._ctrl_cfg = _CTRL_CFG_RESET
        self._saturation_cfg = 0
        self._now_ns = 0
        self._fifo = 0  # samples waiting
        self._next_ns = None
        self._pending = False
        self._update_irq()

    def ranges(self):
        return [("regs", _SIZE)]

    def read(self, range_name, offset, size):
        if offset == _CONFIG:
            status = (_FIFO_EMPTY if not self._fifo else 0) | (_FIFO_FULL if self._fifo >= _DEPTH else 0)
            return self._config | status
        if offset in (_DATA, _DAT_AFTER_STA):
            return self._pop(_DAT_AFTER_STA_EMPTY if offset == _DAT_AFTER_STA else 0)
        if offset == _CTRL_CFG:
            return self._ctrl_cfg
        if offset == _SATURATION_CFG:
            return self._saturation_cfg
        return 0

    def write(self, range_name, offset, size, value):
        value &= 0xFFFFFFFF
        if offset == _CONFIG:
            self._write_config(value)
        elif offset == _CTRL_CFG:
            self._ctrl_cfg = value
        elif offset == _SATURATION_CFG:
            self._saturation_cfg = value
        self._update_irq()

    def next_event_ns(self):
        return self._next_ns

    def tick(self, now_ns):
        self._now_ns = now_ns
        while self._next_ns is not None and now_ns >= self._next_ns:
            if self._fifo < _DEPTH:
                self._fifo += 1
                if self._fifo >= max(self._ctrl_cfg & 0x1F, 1):
                    self._pending = True
            self._next_ns += _SAMPLE_NS
        self._update_irq()

    def _write_config(self, value):
        was_enabled = bool(self._config & _CHNL_EN)
        self._config = value & ~(_STATUS | _INT_CLR)
        if value & _INT_CLR:
            self._pending = False
        enabled = bool(value & _CHNL_EN)
        if enabled and not was_enabled:
            self._next_ns = self._now_ns + _SAMPLE_NS
        elif was_enabled and not enabled:
            self._next_ns = None
            self._fifo = 0

    def _pop(self, when_empty):
        if not self._fifo:
            return when_empty
        self._fifo -= 1
        return self._sample

    def _update_irq(self):
        if self._irq is not None:
            self._irq.set(self._pending)
