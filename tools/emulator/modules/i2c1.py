"""I2C1 controller. Behaviour: docs/hardware/i2c.md. Layout: soc/include/soc/i2c.h.

A byte-at-a-time master with no FIFO. Every bus phase (the address byte, a data byte)
takes nine bit times of the bus clock; when it ends the controller sets `si` and waits
until software clears it. The devices on the bus are objects from `i2c_devices`, added
with `attach()`.

One deliberate difference from the documented hardware: `sto` is a one-shot command here
and reads back 0 once the STOP has been issued. The driver sets it in its working copy of
the register and writes that copy back for the rest of the transfer, and a latched `sto`
would turn the next transfer's START into a STOP.
"""

import logging

from core.module import Module

log = logging.getLogger("emulator.i2c")

_CONFIG, _DATA = 0x00, 0x04
_ENSMB, _STA, _STO, _ACK_TX, _TX_MODE = 1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4
_FREQ_SHIFT, _FREQ_MASK = 6, 0x3FF
_SI, _ACK_RX, _ACK_REQ, _BUSY = 1 << 16, 1 << 17, 1 << 18, 1 << 19
_WRITABLE = _ENSMB | _STA | _ACK_TX | _TX_MODE | _FREQ_MASK << _FREQ_SHIFT | _SI
_IDLE, _ADDRESSED, _DATA_PHASE = "idle", "addressed", "data"


class I2c1Module(Module):
    """`irq` is the controller's ICU line; `ref_hz` the clock the divider divides."""

    def __init__(self, name, clock=None, irq=None, ref_hz=26_000_000, **params):
        super().__init__(name, **params)
        self._clock = clock
        self._irq = irq
        self._ref_hz = ref_hz
        self._devices = {}
        self.reset()

    def attach(self, device):
        if device.address in self._devices:
            raise ValueError(
                "I2C1 address 0x%02X is used twice (%s and %s)"
                % (device.address, self._devices[device.address].NAME, device.NAME)
            )
        self._devices[device.address] = device

    def reset(self):
        self._config = 0  # the writable fields
        self._data = 0
        self._ack_rx = self._ack_req = self._busy = False
        self._phase = _IDLE
        self._device = None
        self._event = None  # (due_ns, kind)
        self._level = False
        self._drive()

    def ranges(self):
        return [("regs", 8)]

    def read(self, range_name, offset, size):
        if offset == _DATA:
            return self._data
        status = (_ACK_RX if self._ack_rx else 0) | (_ACK_REQ if self._ack_req else 0) | (_BUSY if self._busy else 0)
        return self._config | status

    def write(self, range_name, offset, size, value):
        if offset == _DATA:
            self._data = value & 0xFF
        elif offset == _CONFIG:
            self._write_config(value)

    def next_event_ns(self):
        return self._event[0] if self._event else None

    def tick(self, now_ns):
        if self._event and now_ns >= self._event[0]:
            kind = self._event[1]
            self._event = None
            getattr(self, "_done_" + kind)()
            self._drive()

    # ---- commands

    def _write_config(self, value):
        previous = self._config
        self._config = value & _WRITABLE
        log.debug("config <= 0x%08X (%s)", value, _describe(value))
        if not self._config & _ENSMB:
            self._abort()
            self._config &= ~_SI
            self._drive()
            return
        if value & _STO and self._phase != _IDLE:
            self._stop()
        elif value & _STA and not previous & _STA:
            self._start()
        elif previous & _SI and not self._config & _SI and self._phase != _IDLE:
            self._ack_req = False
            self._continue()
        self._drive()

    def _byte_ns(self):
        divider = self._config >> _FREQ_SHIFT & _FREQ_MASK
        return max(9 * (6 + 3 * (divider + 1)) * 1_000_000_000 // self._ref_hz, 1)

    def _schedule(self, kind, duration_ns):
        self._event = (self._clock.now_ns() + duration_ns, kind)

    def _start(self):
        self._busy = True
        self._phase = _IDLE
        self._schedule("address", self._byte_ns())

    def _stop(self):
        if self._device is not None:
            self._device.stop()
            self._device = None
        self._phase = _IDLE
        self._config &= ~_SI
        self._ack_req = False
        self._schedule("stop", self._byte_ns() // 4)

    def _abort(self):
        if self._device is not None:
            self._device.stop()
        self._device = None
        self._phase = _IDLE
        self._event = None
        self._ack_rx = self._ack_req = self._busy = False

    def _continue(self):
        if self._config & _TX_MODE:
            self._schedule("send", self._byte_ns())
        elif self._phase == _ADDRESSED or self._config & _ACK_TX:
            self._schedule("receive", self._byte_ns())

    # ---- what happens when a bus phase ends

    def _done_address(self):
        address, read = self._data >> 1, self._data & 1
        self._device = self._devices.get(address)
        self._ack_rx = self._device is not None and self._device.begin(bool(read))
        log.debug("address 0x%02X %s: %s", address, "read" if read else "write", "ack" if self._ack_rx else "nack")
        if not self._ack_rx:
            self._device = None
        self._phase = _ADDRESSED
        self._config |= _SI

    def _done_send(self):
        self._ack_rx = self._device is not None and self._device.write(self._data)
        log.debug("write 0x%02X: %s", self._data, "ack" if self._ack_rx else "nack")
        self._phase = _DATA_PHASE
        self._config |= _SI

    def _done_receive(self):
        self._data = self._device.read() & 0xFF if self._device is not None else 0xFF
        log.debug("read 0x%02X", self._data)
        self._ack_req = True
        self._phase = _DATA_PHASE
        self._config |= _SI

    def _done_stop(self):
        self._busy = False

    def _drive(self):
        level = bool(self._config & _SI)
        if level != self._level:
            self._level = level
            log.debug("interrupt line %s", "high" if level else "low")
        if self._irq is not None:
            self._irq.set(level)


def _describe(value):
    names = [("ensmb", _ENSMB), ("sta", _STA), ("sto", _STO), ("ack_tx", _ACK_TX), ("tx_mode", _TX_MODE), ("si", _SI)]
    return " ".join([name for name, bit in names if value & bit] + ["div=%d" % (value >> _FREQ_SHIFT & _FREQ_MASK)])
