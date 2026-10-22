"""MAC frame DMA: the transmit queues and the receive chain, as the firmware sets them up.

Descriptor layouts follow the LMAC's hal_desc.h; every descriptor starts with a marker word
the engine checks. Transmit: a queue kick walks the chain of header descriptors (THD) from
the head pointer, gathers each frame from its buffer descriptors (TBD), hands it to the
air, and a little later marks it done and raises the queue's trigger. Receive: a frame is
written into the buffers of the payload descriptor chain (RBD) and its header descriptor
(RHD) gets the length, time stamp, vectors and status.

Frames cross the engine in the clear; there is no encryption or aggregation.
"""

import logging
import struct

log = logging.getLogger("emulator.wifi.frames")

_THD, _TBD, _RHD, _RBD = 0xCAFEBABE, 0xCAFEFADE, 0xBAADF00D, 0xC0DEDBAD
_THD_FMT, _THD_SIZE = "<17I", 68
_TBD_FMT, _TBD_SIZE = "<5I", 20
_RHD_FMT, _RHD_SIZE = "<17I", 68
_RBD_FMT, _RBD_SIZE = "<5I", 20

_FCS = 4
_TX_INTERRUPT_EN = 1 << 8  # in macctrlinfo2
_TX_DESC_DONE, _TX_FRAME_OK = 1 << 31, 1 << 23
_TBD_DONE = 1 << 31
_TX_AIR_NS = 150_000
_MAX_CHAIN = 64
_MAX_FRAME = 4096
_RX_QUEUE_LIMIT = 64
_RX_PD_LAST, _RX_PD_DONE = 0x0001, 0x0002
_RX_HD_DONE, _RX_HD_SUCCESS, _RX_HD_GROUP = 0x4000, 0x2000, 0x0400
_RX_HD_KEY_VALID, _RX_HD_KEY_SHIFT = 0x02000000, 15
_RX_TRIGGERS = 1 << 16 | 1 << 17 | 1 << 19

# queue -> TX trigger bit of the TX/RX status register (queue 4 is the beacon queue)
_TX_TRIGGER = {0: 1 << 6, 1: 1 << 7, 2: 1 << 8, 3: 1 << 9, 4: 1 << 10}


class FrameEngine:
    """`on_transmit(frame)` receives what the firmware sends; `raise_status(bits)` sets bits
    in the TX/RX interrupt status."""

    def __init__(self, memory, on_transmit, raise_status):
        self._memory = memory
        self._on_transmit = on_transmit
        self._raise = raise_status
        self.reset()

    def reset(self):
        self._tx_pending = []  # (done_ns, queue, thd, wants_interrupt)
        self._tx_last = {}  # queue -> last descriptor sent, to find a chain appended later
        self._rhd = self._rbd = 0
        self._last_rhd = self._last_rbd = 0
        self._rx_queue = []

    # ---- transmit

    def transmit(self, queue, head, now_ns):
        """Send the descriptors chained from `head`; with no head, continue after the last one."""
        thd = head or self._follow(self._tx_last.get(queue))
        for _ in range(_MAX_CHAIN):
            if not thd or not self._send_one(queue, thd, now_ns):
                return
            self._tx_last[queue] = thd
            thd = self._word(thd + 4)

    def _follow(self, last):
        return self._word(last + 4) if last else 0

    def _send_one(self, queue, thd, now_ns):
        fields = self._unpack(_THD_FMT, thd, _THD_SIZE)
        if fields is None or fields[0] != _THD:
            log.warning("transmit descriptor at 0x%08X is not valid", thd)
            return False
        first_tbd, start, end, length = fields[3:7]
        control = fields[14]
        data = b""
        if start and _MAX_FRAME > end - start >= 0:
            data += self._read(start, end - start + 1) or b""
        tbd = first_tbd
        for _ in range(32):
            buffer = self._unpack(_TBD_FMT, tbd, _TBD_SIZE) if tbd else None
            if buffer is None or buffer[0] != _TBD:
                break
            if _MAX_FRAME > buffer[3] - buffer[2] >= 0:
                data += self._read(buffer[2], buffer[3] - buffer[2] + 1) or b""
            self._store(tbd + 16, buffer[4] | _TBD_DONE)
            tbd = buffer[1]
        if not data:
            return False  # payload still being downloaded: the firmware kicks again
        frame = data[:length - _FCS] if length > _FCS else data
        self._tx_pending.append((now_ns + _TX_AIR_NS, queue, thd, bool(control & _TX_INTERRUPT_EN)))
        self._on_transmit(frame)
        return True

    def complete(self, now_ns):
        """Mark finished transmissions done and raise their triggers."""
        due = [item for item in self._tx_pending if item[0] <= now_ns]
        self._tx_pending = [item for item in self._tx_pending if item[0] > now_ns]
        for _, queue, thd, wants_interrupt in due:
            status = self._word(thd + 60)
            self._store(thd + 60, (status or 0) | _TX_DESC_DONE | _TX_FRAME_OK)
            self._store(thd + 64, _TX_AIR_NS // 1000)
            if wants_interrupt:
                self._raise(_TX_TRIGGER[queue])

    def next_event_ns(self):
        return min((item[0] for item in self._tx_pending), default=None)

    # ---- receive

    def set_rx_heads(self, header=None, payload=None):
        """The firmware loaded a new head pointer; frames that were waiting may now fit."""
        if header is not None:
            self._rhd, self._last_rhd = header & ~3, 0
        if payload is not None:
            self._rbd, self._last_rbd = payload & ~3, 0
        self._flush()

    def receive(self, frame, tsf, rssi, key_index):
        self._rx_queue.append((bytes(frame), tsf, rssi, key_index))
        self._flush()

    def _flush(self):
        delivered = False
        while self._rx_queue and self._deliver(*self._rx_queue[0]):
            self._rx_queue.pop(0)
            delivered = True
        del self._rx_queue[:-_RX_QUEUE_LIMIT]
        if delivered:
            self._raise(_RX_TRIGGERS)

    def _deliver(self, frame, tsf, rssi, key_index):
        rhd = self._rhd or self._follow(self._last_rhd)
        header = self._unpack(_RHD_FMT, rhd, _RHD_SIZE) if rhd else None
        if header is None or header[0] != _RHD or header[16] & _RX_HD_DONE:
            return False
        buffers = self._claim_buffers(len(frame))
        if buffers is None:
            return False
        position = 0
        for index, (address, descriptor, take) in enumerate(buffers):
            self._write(descriptor[2], frame[position:position + take])
            position += take
            last = _RX_PD_LAST if index == len(buffers) - 1 else 0
            self._store(address + 16, descriptor[4] & 0xFFFF0000 | _RX_PD_DONE | last)
        self._last_rbd = buffers[-1][0]
        self._rbd = buffers[-1][1][1]
        status = _RX_HD_DONE | _RX_HD_SUCCESS
        if len(frame) >= 5 and frame[4] & 1:
            status |= _RX_HD_GROUP
        if key_index is not None:
            status |= _RX_HD_KEY_VALID | (key_index & 0x3FF) << _RX_HD_KEY_SHIFT
        vector_a = (len(frame) + _FCS) & 0xFFF  # legacy rate code 0: 1 Mbit/s
        vector_c = rssi & 0xFF | (rssi & 0xFF) << 24
        words = [
            header[0], header[1], buffers[0][0], header[3], header[4], header[5], header[6],
            len(frame) & 0xFFFF, tsf & 0xFFFFFFFF, tsf >> 32 & 0xFFFFFFFF,
            vector_a, 0, vector_c, 0, 0, 0, status,
        ]
        self._write(rhd, struct.pack(_RHD_FMT, *words))
        self._last_rhd = rhd
        self._rhd = header[1]
        return True

    def _claim_buffers(self, size):
        """Payload descriptors that hold `size` bytes: [(address, fields, bytes taken)], or None."""
        address = self._rbd or self._follow(self._last_rbd)
        claimed, need = [], size
        while need > 0 and address:
            fields = self._unpack(_RBD_FMT, address, _RBD_SIZE)
            if fields is None or fields[0] != _RBD or fields[3] < fields[2]:
                return None
            capacity = fields[3] - fields[2] + 1
            claimed.append((address, fields, min(capacity, need)))
            need -= capacity
            address = fields[1]
        return claimed if need <= 0 and claimed else None

    # ---- guest memory; a descriptor pointing nowhere is skipped, never fatal

    def _read(self, address, size):
        try:
            return bytes(self._memory.read(address, size))
        except Exception:
            return None

    def _write(self, address, data):
        try:
            self._memory.write(address, data)
        except Exception:
            log.warning("cannot write the descriptor or buffer at 0x%08X", address)

    def _word(self, address):
        data = self._read(address, 4)
        return struct.unpack("<I", data)[0] if data else 0

    def _store(self, address, value):
        self._write(address, struct.pack("<I", value & 0xFFFFFFFF))

    def _unpack(self, fmt, address, size):
        data = self._read(address, size)
        return struct.unpack(fmt, data) if data else None
