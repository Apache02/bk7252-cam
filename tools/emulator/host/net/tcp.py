"""The host stack's side of one TCP connection that it opens towards the firmware.

A small TCP: active open, in-order receive, a send buffer limited by the peer's window,
go-back-N retransmission on a timer, FIN from either side, RST. Time is the device clock
in nanoseconds, passed in by the caller. Out-of-order segments are dropped and re-acked;
the peer retransmits. There is no congestion control, no zero-window probe and no options
beyond MSS.
"""

from host.net import packets as p

RTO_NS = 1_000_000_000
MAX_TRIES = 6
OUR_MSS = 1400
DEFAULT_PEER_MSS = 536
RECEIVE_WINDOW = 16384
SEND_BUFFER_LIMIT = 65536
_SEQ_MASK = 0xFFFFFFFF
_SYN_SENT, _OPEN, _CLOSED = "syn-sent", "open", "closed"


class TcpFlow:
    """`emit(segment)` gets each TCP segment to send; `deliver(data)` the bytes the peer sent;
    `on_peer_closed()` its FIN; `on_closed(reset)` the end of the connection."""

    def __init__(self, local_ip, local_port, remote_ip, remote_port, iss, emit, deliver, on_peer_closed, on_closed):
        self._local = (local_ip, local_port)
        self._remote = (remote_ip, remote_port)
        self._emit, self._deliver = emit, deliver
        self._on_peer_closed, self._on_closed = on_peer_closed, on_closed
        self._state = _CLOSED
        self._ended = False
        self._snd_una = (iss + 1) & _SEQ_MASK  # the first data byte, once the SYN is acked
        self._iss = iss
        self._rcv_nxt = 0
        self._buffer = bytearray()  # unacked then unsent bytes; byte i has sequence _snd_una + i
        self._sent = 0  # how many bytes of the buffer are in flight
        self._want_fin = self._fin_sent = self._fin_acked = self._peer_fin = False
        self._window = 0
        self._mss = DEFAULT_PEER_MSS
        self._deadline = None
        self._tries = 0

    @property
    def established(self):
        return self._state == _OPEN

    @property
    def finished(self):
        return self._ended

    def can_write(self):
        return len(self._buffer) < SEND_BUFFER_LIMIT

    def next_event_ns(self):
        return None if self._ended else self._deadline

    # ---- the application side

    def open(self, now):
        self._state = _SYN_SENT
        self._tries = 1
        self._send(self._iss, 0, p.TCP_SYN, mss=OUR_MSS)
        self._deadline = now + RTO_NS

    def write(self, data, now):
        self._buffer += data
        self._transmit(now)

    def close(self, now):
        """No more data to send: a FIN follows the buffered bytes."""
        self._want_fin = True
        self._transmit(now)

    def abort(self):
        if self._state == _OPEN:
            self._send(self._seq_next(), self._rcv_nxt, p.TCP_RST | p.TCP_ACK)
        self._end(None)

    # ---- the network side

    def receive(self, segment, now):
        if self._ended:
            return
        if segment.flags & p.TCP_RST:
            if self._state == _OPEN or segment.ack == (self._iss + 1) & _SEQ_MASK:
                self._end(True)
            return
        if self._state == _SYN_SENT:
            self._receive_syn_ack(segment, now)
        elif self._state == _OPEN:
            self._receive_open(segment, now)

    def _receive_syn_ack(self, segment, now):
        if segment.flags & (p.TCP_SYN | p.TCP_ACK) != p.TCP_SYN | p.TCP_ACK:
            return
        if segment.ack != (self._iss + 1) & _SEQ_MASK:
            self._send(segment.ack, 0, p.TCP_RST)
            self._end(True)
            return
        self._state = _OPEN
        self._rcv_nxt = (segment.seq + 1) & _SEQ_MASK
        self._window = segment.window
        self._mss = min(OUR_MSS, segment.mss or DEFAULT_PEER_MSS)
        self._tries = 0
        self._deadline = None
        self._send_ack()
        self._transmit(now)

    def _receive_open(self, segment, now):
        if segment.flags & p.TCP_ACK:
            self._on_ack(segment, now)
        needs_ack = bool(segment.data) or bool(segment.flags & p.TCP_FIN)
        data = segment.data
        offset = (self._rcv_nxt - segment.seq) & _SEQ_MASK
        if data and offset < 0x80000000 and offset < len(data):
            self._rcv_nxt = (self._rcv_nxt + len(data) - offset) & _SEQ_MASK
            self._deliver(bytes(data[offset:]))
        if segment.flags & p.TCP_FIN and not self._peer_fin and (segment.seq + len(data)) & _SEQ_MASK == self._rcv_nxt:
            self._peer_fin = True
            self._rcv_nxt = (self._rcv_nxt + 1) & _SEQ_MASK
            self._on_peer_closed()
        if needs_ack:
            self._send_ack()
        if self._peer_fin and self._fin_acked:
            self._end(False)

    def _on_ack(self, segment, now):
        self._window = segment.window
        acked = (segment.ack - self._snd_una) & _SEQ_MASK
        outstanding = self._sent + (1 if self._fin_sent and not self._fin_acked else 0)
        if 0 < acked <= outstanding:
            data_acked = min(acked, self._sent)
            del self._buffer[:data_acked]
            self._sent -= data_acked
            self._snd_una = (self._snd_una + data_acked) & _SEQ_MASK
            if acked > data_acked:
                self._fin_acked = True
            self._tries = 0
            self._deadline = now + RTO_NS if self._in_flight() else None
        self._transmit(now)

    def tick(self, now):
        if self._ended or self._deadline is None or now < self._deadline:
            return
        if self._tries >= MAX_TRIES:
            self._end(True)
            return
        self._tries += 1
        self._deadline = now + (RTO_NS << min(self._tries - 1, 5))
        if self._state == _SYN_SENT:
            self._send(self._iss, 0, p.TCP_SYN, mss=OUR_MSS)
            return
        self._sent = 0  # go back: everything unacked is sent again
        self._fin_sent = self._fin_acked
        self._transmit(now)

    # ---- sending

    def _in_flight(self):
        return self._sent > 0 or (self._fin_sent and not self._fin_acked)

    def _seq_next(self):
        return (self._snd_una + self._sent + (1 if self._fin_sent else 0)) & _SEQ_MASK

    def _transmit(self, now):
        if self._state != _OPEN:
            return
        while self._sent < len(self._buffer):
            room = self._window - self._sent
            if room <= 0:
                break
            size = min(self._mss, len(self._buffer) - self._sent, room)
            last = self._sent + size == len(self._buffer)
            flags = p.TCP_ACK | (p.TCP_PSH if last else 0)
            self._send((self._snd_una + self._sent) & _SEQ_MASK, self._rcv_nxt, flags,
                       self._buffer[self._sent:self._sent + size])
            self._sent += size
        if self._want_fin and not self._fin_sent and self._sent == len(self._buffer):
            self._send(self._seq_next(), self._rcv_nxt, p.TCP_FIN | p.TCP_ACK)
            self._fin_sent = True
        if self._in_flight() and self._deadline is None:
            self._deadline = now + RTO_NS

    def _send_ack(self):
        self._send(self._seq_next(), self._rcv_nxt, p.TCP_ACK)

    def _send(self, seq, ack, flags, data=b"", mss=None):
        self._emit(
            p.tcp(self._local[0], self._remote[0], self._local[1], self._remote[1], seq, ack, flags,
                  RECEIVE_WINDOW, bytes(data), mss)
        )

    def _end(self, reset):
        self._ended = True
        self._state = _CLOSED
        self._deadline = None
        if reset is not None:
            self._on_closed(reset)
