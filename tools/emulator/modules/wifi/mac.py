"""The MAC: core (0xC0000000) and platform (0xC0008000) blocks as one module.

The two register banks (mac_core.py, mac_pl.py) hold the registers; this module adds what
the hardware does with them, modelled after the RivieraWaves nxmac interface the firmware
drives (reg_mac_core.h, reg_mac_pl.h):

- STATE_CNTRL: the next state is taken at once; leaving a state for IDLE raises the IDLE
  general interrupt (the firmware waits for it before a channel switch or scan step).
- Interrupts: general and TX/RX status, acknowledge and enable registers. Six ICU lines
  follow (status & enable) of those, split the way the hardware splits them.
- ABS timers 0-9: compared with the microsecond counter; an expired, unmasked timer sets
  its bit in the timers event register and raises the ABS timers general interrupt.
- TSF: the microsecond counter plus a software-writable offset.
- TBTT: while ACTIVE or DOZE, every beacon interval of the TSF raises the TBTT interrupt.
- Key storage RAM: read, write and search commands complete at once.
- Time-on-air calculator: a duration from length and rate.

Frame transmit and receive are not modelled.
"""

import struct

from core.module import Module
from modules.wifi.mac_core import MacCore
from modules.wifi.mac_pl import MacPl

# core registers
_VERSION_1 = 0x004
_STATE_CNTRL = 0x038
_BCN_CNTRL_1 = 0x064
_ENCR_KEYS = (0x0AC, 0x0B0, 0x0B4, 0x0B8)
_ENCR_ADDR_LO, _ENCR_ADDR_HI = 0x0BC, 0x0C0
_ENCR_CNTRL = 0x0C4
_ERR_REC_CNTRL = 0x054
_ABS_TIMER, _ABS_TIMERS = 0x128, 10
# platform registers
_DOZE_CNTRL_2 = 0x048
_GEN_STATUS, _GEN_ACK, _GEN_ENABLE = 0x06C, 0x070, 0x074
_TXRX_STATUS, _TXRX_ACK, _TXRX_ENABLE = 0x078, 0x07C, 0x080
_EVENT_SET, _EVENT_CLEAR, _UNMASK = 0x084, 0x088, 0x08C
_TSF_LO, _TSF_HI = 0x0A4, 0x0A8
_TOA_PARAM_1, _TOA_PARAM_2, _TOA_VALUE = 0x160, 0x164, 0x168
_DMA_STATUS_1 = 0x188

_VERSION_VALUE = 0x00001C03  # no BFMER, no MU-MIMO
_IDLE, _DOZE, _ACTIVE = 0, 2, 3
_MASTER_INT_EN = 1 << 31
_STATUS_BITS = 0x7FFFFFFF
_GEN_TBTT, _GEN_IDLE, _GEN_TIMERS = 1 << 0, 1 << 2, 1 << 3
_KEY_READ, _KEY_WRITE, _KEY_SEARCH, _KEY_NOT_FOUND = 1 << 31, 1 << 30, 1 << 29, 1 << 28
_COMPUTE_DURATION, _DURATION_VALID = 1 << 31, 1 << 30
_TU_US = 1024
_MASK_32 = 0xFFFFFFFF
_MASK_64 = (1 << 64) - 1

# ICU line name -> TX/RX status bits that drive it
_TXRX_LINES = (
    ("prot_trigger", 0x0000003F),
    ("tx_trigger", 0x00009FC0),
    ("rx_trigger", 1 << 16),
    ("tx_rx_timer", 1 << 14 | 1 << 17),
    ("tx_rx_misc", 0x1FFC2000 & ~(1 << 14 | 1 << 16 | 1 << 17)),
)

_LEGACY_MBPS = (1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54)
_HT_MBPS = (6.5, 13, 19.5, 26, 39, 52, 58.5, 65)

_CORE_WORDS = {_VERSION_1, _STATE_CNTRL, _ENCR_CNTRL, _ERR_REC_CNTRL}
_CORE_WORDS |= {_ABS_TIMER + 4 * i for i in range(_ABS_TIMERS)}
_PL_WORDS = {
    _DOZE_CNTRL_2, _GEN_STATUS, _GEN_ACK, _GEN_ENABLE, _TXRX_STATUS, _TXRX_ACK, _TXRX_ENABLE,
    _EVENT_SET, _EVENT_CLEAR, _UNMASK, _TSF_LO, _TSF_HI, _TOA_VALUE, _DMA_STATUS_1,
}


class _Bank:
    """Mixin for a register bank whose behaviour registers are served by the owner."""

    WORDS = frozenset()

    def attach(self, owner):
        self._owner = owner
        return self

    def load(self, offset, size):
        word = offset & ~3
        value = self._owner.served(self, word) if word in self.WORDS else None
        if value is None:
            return super().load(offset, size)
        return value >> (offset & 3) * 8 & ((1 << size * 8) - 1)

    def store(self, offset, size, value):
        word = offset & ~3
        if word not in self.WORDS:
            return super().store(offset, size, value)
        shift = (offset & 3) * 8
        mask = ((1 << size * 8) - 1) << shift
        merged = self.peek(word) & ~mask | (value << shift) & mask
        self.poke(word, self._owner.written(self, word, merged))
        self._owner.settled()


class _CoreBank(_Bank, MacCore):
    WORDS = frozenset(_CORE_WORDS)


class _PlBank(_Bank, MacPl):
    WORDS = frozenset(_PL_WORDS)


class Mac(Module):
    """`irq` maps the line names prot_trigger, tx_trigger, rx_trigger, tx_rx_timer,
    tx_rx_misc and general to ICU lines."""

    def __init__(self, name, clock=None, mac_powered=None, irq=None, **params):
        super().__init__(name, **params)
        self._clock = clock
        self._irq = irq or {}
        self._core = _CoreBank("MAC_CORE", clock=clock, mac_powered=mac_powered).attach(self)
        self._pl = _PlBank("MAC_PL").attach(self)
        self.reset()

    def reset(self):
        self._core.reset()
        self._pl.reset()
        self._state = self._next_state = _IDLE
        self._general = self._txrx = 0
        self._events = 0
        self._armed = [False] * _ABS_TIMERS
        self._targets = [0] * _ABS_TIMERS
        self._tsf_offset = 0
        self._last_tbtt = None
        self._keys = {}
        self._lines = {}
        self._drive_lines()

    def ranges(self):
        return [("core", self._core.SIZE), ("pl", self._pl.SIZE)]

    def read(self, range_name, offset, size):
        return self._bank(range_name).read("regs", offset, size)

    def write(self, range_name, offset, size, value):
        self._bank(range_name).write("regs", offset, size, value)

    def _bank(self, range_name):
        return self._core if range_name == "core" else self._pl

    def tick(self, now_ns):
        self._core.tick(now_ns)
        self._pl.tick(now_ns)
        us = self._core.microseconds()
        if us is None:
            return
        now = us & _MASK_32
        fired = 0
        for i in range(_ABS_TIMERS):
            if self._armed[i] and (now - self._targets[i]) & _MASK_32 < 0x80000000:
                self._armed[i] = False
                fired |= 1 << i
        fired &= self._pl.peek(_UNMASK)  # a masked timer expires silently
        if fired:
            self._events |= fired
            self._general |= _GEN_TIMERS
        period = self._beacon_period_us()
        if period and self._state in (_ACTIVE, _DOZE):
            index = self._tsf(us) // period
            if self._last_tbtt is None:
                self._last_tbtt = index
            elif index != self._last_tbtt:
                self._last_tbtt = index
                if self._state == _DOZE:
                    self._state = self._next_state = _ACTIVE
                self._general |= _GEN_TBTT
        self._drive_lines()

    def next_event_ns(self):
        us = self._core.microseconds()
        if us is None:
            return None
        waits = [(self._targets[i] - us) & _MASK_32 for i in range(_ABS_TIMERS) if self._armed[i]]
        period = self._beacon_period_us()
        if period and self._state in (_ACTIVE, _DOZE):
            tsf = self._tsf(us)
            waits.append((tsf // period + 1) * period - tsf)
        return self._clock.now_ns() + min(waits) * 1000 if waits else None

    def _beacon_period_us(self):
        return (self._core.peek(_BCN_CNTRL_1) & 0xFFFF) * _TU_US

    def _tsf(self, us):
        return (us + self._tsf_offset) & _MASK_64

    def _drive_lines(self):
        general_on = bool(self._pl.peek(_GEN_ENABLE) & _MASTER_INT_EN and self._general_status())
        levels = {"general": general_on}
        txrx = self._txrx_status() if self._pl.peek(_TXRX_ENABLE) & _MASTER_INT_EN else 0
        for name, mask in _TXRX_LINES:
            levels[name] = bool(txrx & mask)
        for name, level in levels.items():
            line = self._irq.get(name)
            if line is not None and self._lines.get(name) != level:
                self._lines[name] = level
                line.set(level)

    def _general_status(self):
        return self._general & self._pl.peek(_GEN_ENABLE) & _STATUS_BITS

    def _txrx_status(self):
        return self._txrx & self._pl.peek(_TXRX_ENABLE) & _STATUS_BITS

    # ---- register behaviour, called by the banks

    def served(self, bank, word):
        """The value a read of a behaviour register returns, or None for the stored one."""
        if bank is self._core:
            if word == _VERSION_1:
                return _VERSION_VALUE
            if word == _STATE_CNTRL:
                return self._next_state << 4 | self._state
            return None
        us = self._core.microseconds()
        if word == _GEN_STATUS:
            return self._general_status()
        if word == _TXRX_STATUS:
            return self._txrx_status()
        if word in (_EVENT_SET, _EVENT_CLEAR):
            return self._events
        if word == _TSF_LO:
            return self._tsf(us or 0) & _MASK_32
        if word == _TSF_HI:
            return self._tsf(us or 0) >> 32
        if word == _DMA_STATUS_1:
            return 0  # every DMA state machine idle
        return None

    def written(self, bank, word, value):
        """Apply a write to a behaviour register; returns the word to store."""
        if bank is self._core:
            return self._core_written(word, value)
        return self._pl_written(word, value)

    def _core_written(self, word, value):
        if word == _STATE_CNTRL:
            self._set_state(value >> 4 & 0xF)
        elif word == _ERR_REC_CNTRL:
            return value & ~0xFC  # FSM and FIFO resets complete at once
        elif word == _ENCR_CNTRL:
            return self._key_command(value)
        elif word >= _ABS_TIMER:
            self._arm_timer((word - _ABS_TIMER) >> 2, value)
        return value

    def _set_state(self, new):
        previous = self._state
        self._next_state = self._state = new
        if new == _IDLE and previous != _IDLE:
            self._general |= _GEN_IDLE
        if new == _ACTIVE and previous != _DOZE:
            self._last_tbtt = None  # re-sync TBTT to the current TSF

    def _arm_timer(self, index, target):
        self._targets[index] = target
        us = self._core.microseconds() or 0
        now = us & _MASK_32
        # A target at or behind the counter never matches; the firmware forces those
        # through the event register itself.
        self._armed[index] = target != now and (target - now) & _MASK_32 < 0x80000000

    def _pl_written(self, word, value):
        if word == _GEN_ACK:
            self._general &= ~value
        elif word == _TXRX_ACK:
            self._txrx &= ~value
        elif word == _EVENT_SET:
            self._events |= value & 0x3FF
            if self._events & self._pl.peek(_UNMASK):
                self._general |= _GEN_TIMERS
            return self._events
        elif word == _EVENT_CLEAR:
            self._events &= ~value
            return self._events
        elif word in (_TSF_LO, _TSF_HI):
            self._set_tsf(word, value)
        elif word == _DOZE_CNTRL_2:
            if value & 1 and self._state == _DOZE:
                self._state = self._next_state = _ACTIVE
        elif word == _TOA_VALUE:
            if value & _COMPUTE_DURATION:
                return self._time_on_air()
        return value

    def settled(self):
        """After a behaviour register was stored: the interrupt lines may have changed."""
        self._drive_lines()

    def _set_tsf(self, word, value):
        us = self._core.microseconds() or 0
        tsf = self._tsf(us)
        if word == _TSF_LO:
            tsf = tsf & ~_MASK_32 | value
        else:
            tsf = value << 32 | tsf & _MASK_32
        self._tsf_offset = (tsf - us) & _MASK_64

    def _time_on_air(self):
        length = self._pl.peek(_TOA_PARAM_1) & 0xFFFFF
        mcs = self._pl.peek(_TOA_PARAM_2) & 0x7F
        if (self._pl.peek(_TOA_PARAM_1) >> 24 & 7) >= 2:
            rate, overhead = _HT_MBPS[min(mcs, len(_HT_MBPS) - 1)], 40
        else:
            rate = _LEGACY_MBPS[min(mcs, len(_LEGACY_MBPS) - 1)]
            overhead = 192 if rate < 6 else 20
        return int(overhead + length * 8 / rate) & 0xFFFF | _DURATION_VALID

    def _key_command(self, value):
        index = value >> 16 & 0x3FF
        address = struct.pack("<IH", self._core.peek(_ENCR_ADDR_LO), self._core.peek(_ENCR_ADDR_HI) & 0xFFFF)
        if value & _KEY_WRITE:
            key = [self._core.peek(offset) for offset in _ENCR_KEYS]
            self._keys[index] = (address, key, value & 0x7FF)  # cipher type, vlan, flags
        elif value & _KEY_READ:
            entry = self._keys.get(index)
            if entry is not None:
                stored_address, key, attributes = entry
                self._core.poke(_ENCR_ADDR_LO, int.from_bytes(stored_address[:4], "little"))
                self._core.poke(_ENCR_ADDR_HI, int.from_bytes(stored_address[4:], "little"))
                for offset, word in zip(_ENCR_KEYS, key):
                    self._core.poke(offset, word)
                value = value & ~0x7FF | attributes
        elif value & _KEY_SEARCH:
            found = next((i for i, entry in self._keys.items() if entry[0] == address), None)
            if found is None:
                value |= _KEY_NOT_FOUND
            else:
                value = value & ~(_KEY_NOT_FOUND | 0x3FF << 16) | found << 16
        return value & ~(_KEY_READ | _KEY_WRITE | _KEY_SEARCH)
