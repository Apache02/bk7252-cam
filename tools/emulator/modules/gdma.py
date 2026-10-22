"""General DMA: six channels. Behaviour: docs/hardware/dma.md; layout: soc/gdma.h.

A channel moves `transfer_length + 1` bytes through a byte accumulator: each source
read fetches a 32-bit word and keeps its low 2**src_width bytes, each destination
write drains 2**dst_width bytes into the low lanes of a 32-bit word. Addresses step
by +4 per access when incrementing. An under-filled last write is zero padded.

Only memory-to-memory channels run. A channel with a peripheral request line, or in
repeat mode, is reported and not emulated (request lines stall, repeat runs once).

The copy happens when the modeled transfer time has passed, so a firmware polling
`enable` sees it set meanwhile. Run with `--debug gdma` for a decoded log of every
channel configuration, start, completion and acknowledge.
"""

import logging

from core.module import BusError, Module

CHANNELS = 6
_CH_STRIDE = 0x20
_INT_COUNTS = 0xC0
_PRIO_MODE = 0xDC
_INT_STATUS = 0xE0
_SRC_PAUSE, _DST_PAUSE, _SRC_RD, _DST_WR = 0x100, 0x120, 0x140, 0x160
_SIZE = 0x200

_CONFIG, _DST_START, _SRC_START, _MUX = 0x00, 0x04, 0x08, 0x1C

_REQUESTS = {
    0x0: "DTCM", 0x1: "HSSPI", 0x2: "AUDIO", 0x3: "SDIO", 0x4: "UART1", 0x5: "UART2",
    0x6: "I2S", 0x7: "GSPI", 0x8: "JPEG", 0x9: "PSRAM_V", 0xA: "PSRAM_A", 0xB: "reserved",
}

log = logging.getLogger("emulator.gdma")


def transfer(memory, src, dst, length, sdw, ddw, sinc, dinc):
    """Run one channel's data movement; returns (reads, writes)."""
    src_bytes, dst_bytes = 1 << sdw, 1 << ddw
    writes = -(-length // dst_bytes)
    accumulator = bytearray()
    taken = reads = 0
    for _ in range(writes):
        while len(accumulator) < dst_bytes and taken < length:
            word = memory.read(src, 4)
            take = min(src_bytes, length - taken)
            accumulator += word[:take]
            taken += take
            reads += 1
            if sinc:
                src += 4
        chunk = bytes(accumulator[:dst_bytes]).ljust(dst_bytes, b"\x00")
        del accumulator[:dst_bytes]
        memory.write(dst, chunk)
        if dinc:
            dst += 4
    return reads, writes


def _timing(length, sdw, ddw, mux):
    """(dst writes, src reads, duration in ns * bus_hz) of a channel.

    A word costs 7.2 bus cycles; from a pacing interval of 6 up the period is
    interval + 2 cycles (docs/hardware/dma.md, "Pacing intervals").
    """
    writes = -(-length // (1 << ddw))
    reads = -(-length // (1 << sdw))
    interval = max(mux >> 12 & 0xF, mux >> 16 & 0xF)
    cycles_x10 = max(72, (interval + 2) * 10) if interval >= 6 else 72
    return writes, reads, max(writes, reads) * cycles_x10 * 100_000_000


class _Channel:
    def __init__(self):
        self.reset()

    def reset(self):
        self.config = 0
        self.dst = self.src = 0
        self.loops = [0, 0, 0, 0]
        self.mux = 0
        self.pause = [0, 0]
        self.running = False
        self.stalled = False
        self.start_ns = self.end_ns = self.half_ns = 0
        self.writes = 0
        self.half_pending = False
        self.fin_count = self.half_count = 0


class GdmaModule(Module):
    def __init__(self, name, clock=None, irq=None, memory=None, bus_hz=120_000_000, **params):
        super().__init__(name, **params)
        self._clock = clock
        self._irq = irq
        self._memory = memory
        self._bus_hz = bus_hz
        self._channels = [_Channel() for _ in range(CHANNELS)]
        self.reset()

    def reset(self):
        for channel in self._channels:
            channel.reset()
        self._fin = 0
        self._half = 0
        self._prio = 0
        self._update_irq()

    def ranges(self):
        return [("regs", _SIZE)]

    def read(self, range_name, offset, size):
        index, within = divmod(offset, _CH_STRIDE)
        if index < CHANNELS:
            return self._read_channel(self._channels[index], within)
        if _INT_COUNTS <= offset < _INT_COUNTS + 4 * CHANNELS:
            return self._int_counts((offset - _INT_COUNTS) // 4)
        if offset == _PRIO_MODE:
            return self._prio
        if offset == _INT_STATUS:
            return self._fin | self._half << 8
        for base, field in ((_SRC_PAUSE, 0), (_DST_PAUSE, 1)):
            if base <= offset < base + 4 * CHANNELS:
                return self._channels[(offset - base) // 4].pause[field]
        if _DST_WR <= offset < _DST_WR + 4 * CHANNELS:
            return self._dst_wr_addr((offset - _DST_WR) // 4)
        return 0

    def write(self, range_name, offset, size, value):
        value &= 0xFFFFFFFF
        index, within = divmod(offset, _CH_STRIDE)
        if index < CHANNELS:
            self._write_channel(index, within, value)
        elif offset == _PRIO_MODE:
            self._prio = value & 1
        elif offset == _INT_STATUS:
            self._acknowledge(value)
        else:
            for base, field in ((_SRC_PAUSE, 0), (_DST_PAUSE, 1)):
                if base <= offset < base + 4 * CHANNELS:
                    self._channels[(offset - base) // 4].pause[field] = value

    def tick(self, now_ns):
        for index, channel in enumerate(self._channels):
            if not channel.running or channel.stalled:
                continue
            if channel.half_pending and now_ns >= channel.half_ns:
                self._half_finished(index, channel)
            if now_ns >= channel.end_ns:
                self._finish(index, channel)
        self._update_irq()

    def next_event_ns(self):
        events = []
        for channel in self._channels:
            if channel.running and not channel.stalled:
                events.append(channel.half_ns if channel.half_pending else channel.end_ns)
                events.append(channel.end_ns)
        return min(events) if events else None

    def _read_channel(self, channel, within):
        if within == _CONFIG:
            return (channel.config & ~1) | int(channel.running)
        if within == _DST_START:
            return channel.dst
        if within == _SRC_START:
            return channel.src
        if within == _MUX:
            return channel.mux
        if 0x0C <= within < 0x1C:
            return channel.loops[(within - 0x0C) // 4]
        return 0

    def _write_channel(self, index, within, value):
        channel = self._channels[index]
        if within == _CONFIG:
            self._write_config(index, channel, value)
        elif within == _DST_START:
            channel.dst = value
        elif within == _SRC_START:
            channel.src = value
        elif within == _MUX:
            channel.mux = value
        elif 0x0C <= within < 0x1C:
            channel.loops[(within - 0x0C) // 4] = value

    def _write_config(self, index, channel, value):
        enable = value & 1
        channel.config = value
        if enable and not channel.running:
            self._start(index, channel)
        elif not enable and channel.running:
            channel.running = False
            channel.stalled = False
            log.debug("ch%d stopped before it finished", index)
            self._update_irq()
        elif value == 0:
            log.debug("ch%d config cleared", index)
        elif not enable:
            log.debug("ch%d config %s", index, self._describe(channel))

    def _start(self, index, channel):
        cfg = channel.config
        length = (cfg >> 16) + 1
        sdw, ddw = cfg >> 4 & 3, cfg >> 6 & 3
        src_req, dst_req = channel.mux & 0xF, channel.mux >> 4 & 0xF
        writes, _, scaled = _timing(length, sdw, ddw, channel.mux)
        now = self._clock.now_ns() if self._clock is not None else 0
        duration = scaled // self._bus_hz
        channel.running = True
        channel.writes = writes
        channel.start_ns = now
        channel.end_ns = now + duration
        channel.half_ns = now + duration // 2
        channel.half_pending = bool(cfg & 4)
        log.debug("ch%d start: %s, est %s", index, self._describe(channel), _human(duration))
        channel.stalled = bool(src_req or dst_req)
        if channel.stalled:
            for side, req in (("source", src_req), ("destination", dst_req)):
                if req:
                    log.warning(
                        "ch%d: %s request line %s is not emulated; the transfer stalls",
                        index, side, _REQUESTS.get(req, "0x%X" % req),
                    )
        if cfg & 8:
            log.warning("ch%d: repeat mode is not emulated; the transfer runs once", index)

    def _half_finished(self, index, channel):
        channel.half_pending = False
        self._half |= 1 << index
        channel.half_count = (channel.half_count + 1) & 0xF
        log.debug("ch%d half-finish flag set", index)

    def _finish(self, index, channel):
        cfg = channel.config
        channel.running = False
        length = (cfg >> 16) + 1
        try:
            transfer(
                self._memory, channel.src, channel.dst, length,
                cfg >> 4 & 3, cfg >> 6 & 3, bool(cfg & 0x100), bool(cfg & 0x200),
            )
        except BusError as err:
            log.error(
                "ch%d aborted: %s (src 0x%08X -> dst 0x%08X, %d bytes)",
                index, err, channel.src, channel.dst, length,
            )
            return
        log.debug("ch%d done: %d bytes copied", index, length)
        if cfg & 2:
            self._fin |= 1 << index
            channel.fin_count = (channel.fin_count + 1) & 0xF
            log.debug("ch%d finish flag set", index)

    def _acknowledge(self, value):
        fin, half = value & 0x3F, value >> 8 & 0x3F
        for index in range(CHANNELS):
            if fin >> index & 1 and self._fin >> index & 1:
                log.debug("ch%d fin flag cleared by software", index)
            if half >> index & 1 and self._half >> index & 1:
                log.debug("ch%d half-fin flag cleared by software", index)
        self._fin &= ~fin
        self._half &= ~half
        self._update_irq()

    def _int_counts(self, index):
        channel = self._channels[index]
        remain = 0
        if channel.running:
            span = max(channel.end_ns - channel.start_ns, 1)
            now = self._clock.now_ns() if self._clock is not None else channel.start_ns
            done = channel.writes * min(max(now - channel.start_ns, 0), span) // span
            remain = channel.writes - done
        return remain | channel.fin_count << 24 | channel.half_count << 28

    def _dst_wr_addr(self, index):
        channel = self._channels[index]
        if not channel.running:
            return channel.dst
        span = max(channel.end_ns - channel.start_ns, 1)
        now = self._clock.now_ns() if self._clock is not None else channel.start_ns
        done = channel.writes * min(max(now - channel.start_ns, 0), span) // span
        return channel.dst + (4 * done if channel.config & 0x200 else 0)

    def _update_irq(self):
        if self._irq is not None:
            self._irq.set(bool(self._fin or self._half))

    @staticmethod
    def _describe(channel):
        cfg, mux = channel.config, channel.mux
        length = (cfg >> 16) + 1
        sdw, ddw = cfg >> 4 & 3, cfg >> 6 & 3
        events = [name for bit, name in ((2, "fin_int"), (4, "half_fin_int")) if cfg & bit]
        notes = []
        if cfg & 8:
            notes.append("repeat")
        interval = (mux >> 12 & 0xF, mux >> 16 & 0xF)
        if any(interval):
            notes.append("interval r%d/w%d" % interval)
        return "%s -> %s, %d bytes (%d writes), src %d-bit %s, dst %d-bit %s%s%s" % (
            _endpoint(mux & 0xF, channel.src), _endpoint(mux >> 4 & 0xF, channel.dst),
            length, -(-length // (1 << ddw)),
            8 << sdw, "+4" if cfg & 0x100 else "fixed",
            8 << ddw, "+4" if cfg & 0x200 else "fixed",
            ", irq: " + "+".join(events) if events else ", irq: none",
            ", " + ", ".join(notes) if notes else "",
        )


def _endpoint(request, address):
    return "%s 0x%08X" % (_REQUESTS.get(request, "req%d" % request), address)


def _human(ns):
    return "%.1f us" % (ns / 1000) if ns < 1_000_000 else "%.2f ms" % (ns / 1_000_000)
