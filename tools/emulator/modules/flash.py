"""SPI flash controller and its CRC-interleaved XIP window.

Raw physical bytes are stored as the chip holds them: every 32 data bytes are
followed by a big-endian CRC16 (docs/hardware/flash.md). The XIP window shows
the decoded view; a block with a bad CRC aborts while `conf.crc_en` is set.
"""

import logging
import struct

from core.module import Fault, Module

DATA = 32
BLOCK = 34
PAGE = 0x1000
CTRL_SIZE = 0x28

_OP_SHIFT, _OP_MASK = 24, 0x1F
_OP_START = 1 << 29
_OP_RDID, _OP_READ, _OP_RDSR, _OP_WREN, _OP_WRDI = 20, 5, 3, 1, 2
_WRITE_OPS = {12: "PP", 13: "SE", 14: "BE1", 15: "BE2", 16: "CE"}
_CRC_EN = 1 << 26

log = logging.getLogger("emulator.flash")


def _crc_table():
    table = []
    for byte in range(256):
        crc = byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x8005) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        table.append(crc)
    return table


_TABLE = _crc_table()


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc = ((crc << 8) & 0xFFFF) ^ _TABLE[(crc >> 8) ^ byte]
    return crc


class FlashModule(Module):
    loadable = True
    demand_ranges = ("xip",)

    def __init__(self, name, size=0x200000, jedec_id=0x1C7015, **params):
        super().__init__(name, **params)
        self._size = size
        self._raw = bytearray(b"\xff" * size)
        self._blocks = size // BLOCK
        self._valid = {}  # block index -> bool, filled on demand
        self._window = (self._blocks * DATA + PAGE - 1) & ~(PAGE - 1)
        self._jedec = jedec_id
        self._operate = 0
        self._conf = _CRC_EN
        self._m_value = 0
        self._sr = 0
        self._fifo = [0] * 8
        self._fifo_ptr = 0
        self._wel = False
        self._last_alert = None

    def ranges(self):
        return [("xip", self._window), ("ctrl", CTRL_SIZE)]

    @property
    def crc_enabled(self):
        return bool(self._conf & _CRC_EN)

    def check_load(self, offset, size):
        if offset % BLOCK or size % BLOCK:
            raise ValueError(
                "flash load of %d bytes at 0x%X must be a multiple of %d and block aligned"
                % (size, offset, BLOCK)
            )
        if offset + size > self._blocks * BLOCK:
            raise ValueError(
                "flash load of %d bytes at 0x%X does not fit the %d byte flash"
                % (size, offset, self._size)
            )

    def load(self, offset, data):
        self.check_load(offset, len(data))
        self._raw[offset:offset + len(data)] = data
        for n in range(offset // BLOCK, (offset + len(data)) // BLOCK):
            self._valid.pop(n, None)
        self.invalidate("xip", 0, self._window)

    def decode_block(self, n):
        """The 32 data bytes of logical block n, or None when its CRC is bad."""
        if not self._block_ok(n):
            return None
        return self._block_data(n)

    def page(self, range_name, offset):
        first = offset // DATA
        out = bytearray()
        bad = []
        for i in range(PAGE // DATA):
            n = first + i
            if n >= self._blocks:
                out += bytes(DATA)
                bad.append((i * DATA, (i + 1) * DATA))
            elif self.crc_enabled and not self._block_ok(n):
                out += bytes(DATA)
                bad.append((i * DATA, (i + 1) * DATA))
            else:
                out += self._block_data(n)
        return bytes(out), _merge(bad)

    def read(self, range_name, offset, size):
        if range_name == "xip":
            return self.xip_read(offset, size)
        return self._ctrl_read(offset)

    def write(self, range_name, offset, size, value):
        if range_name == "ctrl":
            self._ctrl_write(offset, value)

    def xip_read(self, offset, size):
        data = bytearray()
        for addr in range(offset, offset + size):
            n = addr // DATA
            if n >= self._blocks or (self.crc_enabled and not self._block_ok(n)):
                self._alert(offset, n)
                raise Fault("dabt", offset)
            data.append(self._block_data(n)[addr % DATA])
        return int.from_bytes(data, "little")

    def _block_data(self, n):
        start = n * BLOCK
        return bytes(self._raw[start:start + DATA])

    def _block_ok(self, n):
        ok = self._valid.get(n)
        if ok is None:
            start = n * BLOCK
            stored = struct.unpack(">H", self._raw[start + DATA:start + BLOCK])[0]
            ok = stored == crc16(self._raw[start:start + DATA])
            self._valid[n] = ok
        return ok

    def _alert(self, offset, n):
        # A guest stuck re-reading one bad block must not flood the console.
        if n == self._last_alert:
            return
        self._last_alert = n
        log.error(
            "ALERT: XIP read of an unreadable block (bad CRC or past the end of flash): "
            "logical 0x%08X, block %d, physical 0x%06X",
            offset, n, n * BLOCK,
        )

    def _ctrl_read(self, offset):
        reg = offset // 4
        if reg == 0:
            return self._operate & ~(1 << 31) & ~_OP_START
        if reg == 2:
            word = self._fifo[self._fifo_ptr]
            self._fifo_ptr = (self._fifo_ptr + 1) & 7
            return word
        if reg == 4:
            return self._jedec
        if reg == 5:
            return self._sr | (self._fifo_ptr << 16) | (7 << 19) | (self._m_value << 22)
        if reg == 7:
            return self._conf
        return 0

    def _ctrl_write(self, offset, value):
        reg = offset // 4
        if reg == 0:
            self._operate = value
            if value & _OP_START:
                self._command((value >> _OP_SHIFT) & _OP_MASK, value & 0xFFFFFF)
        elif reg == 5:
            self._m_value = (value >> 22) & 0xFF
        elif reg == 7:
            changed = (value ^ self._conf) & _CRC_EN
            self._conf = value
            if changed:
                self.invalidate("xip", 0, self._window)

    def _command(self, op, addr):
        if op == _OP_RDID:
            return
        if op == _OP_RDSR:
            self._sr = 2 if self._wel else 0
        elif op == _OP_WREN:
            self._wel = True
        elif op == _OP_WRDI:
            self._wel = False
        elif op == _OP_READ:
            start = addr & ~(DATA - 1)
            chunk = bytes(self._raw[start:start + DATA]).ljust(DATA, b"\xff")
            self._fifo = list(struct.unpack("<8I", chunk))
            self._fifo_ptr = 0
        elif op in _WRITE_OPS:
            log.warning("flash %s at 0x%06X is not emulated", _WRITE_OPS[op], addr)


def _merge(spans):
    merged = []
    for start, end in spans:
        if merged and merged[-1][1] == start:
            merged[-1] = (merged[-1][0], end)
        else:
            merged.append((start, end))
    return merged
