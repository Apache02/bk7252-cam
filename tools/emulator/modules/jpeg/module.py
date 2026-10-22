"""JPEG encoder / DVP block. Behaviour: docs/hardware/jpeg.md. Layout: soc/jpeg.h.

There is no sensor. While the encoder is enabled, a picture is "captured" once per frame
interval: `start_frame` fills the output FIFO with a finished JPEG (read position back to
the first word), and `end_frame` follows a fixed time later. The rest of the stream
behaves like the chip's: words come out of `rx_fifo_data` until `empty_fifo` is set, and
a DMA channel with the JPEG request line drains it while the line is up.

The picture is the file `picture.jpg` next to this module, read when the module is built;
any JPEG can replace it. The picture size, the quantization table, the sync polarity, the
input format and the bit-rate control have no effect.
"""

import logging
import pathlib
import struct

from core.module import Module

log = logging.getLogger("emulator.jpeg")

_PICTURE = pathlib.Path(__file__).with_name("picture.jpg")
_TRAILER_BYTES = 5

_CTRL0, _CTRL1, _TARGET_H, _TARGET_L = 0x00, 0x04, 0x08, 0x0C
_FIFO, _STATUS, _BYTES, _STATE, _QUANT = 0x14, 0x18, 0x1C, 0x20, 0x80
_START_INT, _END_INT = 1 << 2, 1 << 3
_BYTE_REV, _ENC_EN, _ENC_SIZE = 1 << 1, 1 << 4, 1 << 16
_START, _END = 1, 2
_SIZE = 0x100


def _aligned(jpeg):
    """`jpeg`, with a comment segment after the SOI marker when needed so that it plus
    the trailer fills whole 32-bit words."""
    if (len(jpeg) + _TRAILER_BYTES) % 4 == 0:
        return jpeg
    pad = -(len(jpeg) + 4 + _TRAILER_BYTES) % 4
    return jpeg[:2] + b"\xFF\xFE" + struct.pack(">H", 2 + pad) + bytes(pad) + jpeg[2:]


class JpegModule(Module):
    """`irq` is the ICU line, `dma` the request line into the GDMA. A frame starts every
    `frame_interval_ms` and ends `encode_ms` later. `picture` is the path of the JPEG to
    show, `picture.jpg` by default."""

    def __init__(
        self, name, clock=None, irq=None, dma=None, frame_interval_ms=1000, encode_ms=150, picture=None, **params
    ):
        super().__init__(name, **params)
        self._picture = _aligned(pathlib.Path(picture or _PICTURE).read_bytes())
        self._clock = clock
        self._irq = irq
        self._dma = dma
        self._interval_ns = frame_interval_ms * 1_000_000
        self._encode_ns = encode_ms * 1_000_000
        self._quant = [0] * 32  # the table RAM survives a reset
        if dma is not None:
            dma.bind(lambda: self._pop().to_bytes(4, "little"))
        self.reset()

    def reset(self):
        self._ctrl0 = self._ctrl1 = 0
        self._targets = [0, 0]
        self._status = 0
        self._last_size = 0
        self._frame_size = 0
        self._stream = b""
        self._offset = 0
        self._start_ns = self._end_ns = None
        self._drive()

    def ranges(self):
        return [("regs", _SIZE)]

    def read(self, range_name, offset, size):
        if offset == _CTRL0:
            return self._ctrl0
        if offset == _CTRL1:
            return self._ctrl1
        if offset in (_TARGET_H, _TARGET_L):
            return self._targets[(offset - _TARGET_H) // 4]
        if offset == _FIFO:
            return self._pop()
        if offset == _STATUS:
            return self._status
        if offset == _BYTES:
            return self._last_size
        if offset == _STATE:
            return int(self._offset >= len(self._stream))
        if _QUANT <= offset < _QUANT + 4 * len(self._quant):
            return self._quant[(offset - _QUANT) // 4]
        return 0

    def write(self, range_name, offset, size, value):
        value &= 0xFFFFFFFF
        if offset == _CTRL0:
            self._ctrl0 = value
        elif offset == _CTRL1:
            self._write_ctrl1(value)
        elif offset in (_TARGET_H, _TARGET_L):
            self._targets[(offset - _TARGET_H) // 4] = value
        elif offset == _STATUS:
            self._status &= ~value
        elif _QUANT <= offset < _QUANT + 4 * len(self._quant):
            self._quant[(offset - _QUANT) // 4] = value
        self._drive()

    def next_event_ns(self):
        events = [t for t in (self._start_ns, self._end_ns) if t is not None]
        return min(events) if events else None

    def tick(self, now_ns):
        while True:
            due = self.next_event_ns()
            if due is None or due > now_ns:
                break
            if due == self._end_ns:
                self._end_frame()
            else:
                self._start_frame(due)
        self._drive()

    def _write_ctrl1(self, value):
        was_enabled = bool(self._ctrl1 & _ENC_EN)
        self._ctrl1 = value
        enabled = bool(value & _ENC_EN)
        if enabled and not was_enabled:
            self._start_ns = self._clock.now_ns() + self._interval_ns
            log.debug("encoder enabled, first frame in %d ms", self._interval_ns // 1_000_000)
        elif was_enabled and not enabled:
            self._start_ns = self._end_ns = None
            self._stream, self._offset = b"", 0
            log.debug("encoder disabled")

    def _start_frame(self, at_ns):
        picture = self._picture
        self._frame_size = len(picture)
        if self._ctrl1 & _ENC_SIZE:
            picture += b"\x00" + struct.pack("<I", len(picture))
        self._stream = picture + bytes(-len(picture) % 4)
        self._offset = 0
        self._status |= _START
        self._start_ns = at_ns + self._interval_ns
        self._end_ns = at_ns + self._encode_ns
        log.debug("start_frame: %d bytes", len(self._stream))

    def _end_frame(self):
        self._last_size = self._frame_size
        self._status |= _END
        self._end_ns = None
        log.debug("end_frame: frame is %d bytes", self._last_size)

    def _pop(self):
        """The next word of `rx_fifo_data`; when it is empty, the last frame's size (or 0)."""
        if self._offset >= len(self._stream):
            return self._last_size if self._ctrl1 & _ENC_SIZE else 0
        word = self._stream[self._offset:self._offset + 4]
        self._offset += 4
        self._drive()
        return int.from_bytes(word, "little" if self._ctrl1 & _BYTE_REV else "big")

    def _drive(self):
        if self._irq is not None:
            self._irq.set(bool(
                self._status & _START and self._ctrl0 & _START_INT
                or self._status & _END and self._ctrl0 & _END_INT
            ))
        if self._dma is not None:
            self._dma.set(self._offset < len(self._stream))
