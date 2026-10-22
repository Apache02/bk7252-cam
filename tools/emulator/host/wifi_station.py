"""The far end of the air: a virtual 802.11 station that joins the firmware's access point.

It plays the air for the MAC model: every frame the firmware transmits comes in through
`transmit()`, and what the station sends goes back through the callback given to `attach()`.
It finds the BSS from a beacon, authenticates (open system), associates, and then carries
Ethernet frames between the firmware and the host network stack (host/net/stack.py).

Only open networks are joined. Everything the station sends to the firmware is queued and
released after a short device-time latency: a reply never arrives inside the firmware's own
transmit call, as it would not on air. Time is the device clock in nanoseconds, taken from
`tick()`.
"""

import logging
import struct

log = logging.getLogger("emulator.net.wifi")

LATENCY_NS = 200_000
RETRY_NS = 1_000_000_000
_AUTH_DELAY_NS = 50_000_000

_MGMT, _DATA = 0, 2
_ASSOC_REQ, _ASSOC_RESP, _PROBE_REQ, _PROBE_RESP = 0, 1, 4, 5
_BEACON, _DISASSOC, _AUTH, _DEAUTH = 8, 10, 11, 12
_TO_DS, _FROM_DS = 0x0100, 0x0200
_PRIVACY = 0x0010
_IE_SSID, _IE_RATES, _IE_EXT_RATES = 0, 1, 50
_RATES = bytes([0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24])
_EXT_RATES = bytes([0x30, 0x48, 0x60, 0x6C])
_LLC_SNAP = b"\xaa\xaa\x03\x00\x00\x00"
_BROADCAST = b"\xff" * 6
_SCAN, _AUTHENTICATING, _ASSOCIATING, _RUNNING = "scan", "auth", "assoc", "run"


def _information_elements(data):
    elements, index = {}, 0
    while index + 2 <= len(data) and index + 2 + data[index + 1] <= len(data):
        elements.setdefault(data[index], data[index + 2:index + 2 + data[index + 1]])
        index += 2 + data[index + 1]
    return elements


def _element(eid, payload):
    return bytes([eid, len(payload)]) + payload


class Station:
    """`stack` (set by the caller) is the host network stack: `link_up(now)`,
    `receive(ethernet_frame, now)`, `tick(now)` and `next_event_ns()`."""

    def __init__(self, mac, ssid=None):
        self.mac = mac
        self._ssid = ssid
        self.stack = None
        self._deliver = None
        self._now = 0
        self._seq = 0
        self.reset()

    @property
    def associated(self):
        return self._state == _RUNNING

    def attach(self, deliver):
        self._deliver = deliver

    def reset(self):
        """The chip reset: forget the network and scan again."""
        self._state = _SCAN
        self._bss = None
        self._next = self._now + RETRY_NS
        self._outbox = []
        self._warned = False

    # ---- from the firmware

    def transmit(self, frame):
        if len(frame) < 24:
            return
        kind, subtype = frame[0] >> 2 & 3, frame[0] >> 4
        if kind == _MGMT:
            self._on_management(subtype, frame)
        elif kind == _DATA:
            self._on_data(frame)

    def _on_management(self, subtype, frame):
        body = frame[24:]
        if subtype in (_BEACON, _PROBE_RESP):
            if self._state == _SCAN:
                self._learn(frame[16:22], body)
        elif frame[4:10] != self.mac or self._bss is None or frame[16:22] != self._bss[0]:
            return
        elif subtype == _AUTH and self._state == _AUTHENTICATING and len(body) >= 6:
            _, sequence, status = struct.unpack("<HHH", body[:6])
            if sequence == 2 and status == 0:
                log.debug("authenticated with %s", _mac(self._bss[0]))
                self._state, self._next = _ASSOCIATING, self._now
            elif sequence == 2:
                log.debug("authentication refused, status %d", status)
                self._next = self._now + RETRY_NS
        elif subtype == _ASSOC_RESP and self._state == _ASSOCIATING and len(body) >= 6:
            _, status, aid = struct.unpack("<HHH", body[:6])
            if status == 0:
                self._state, self._next = _RUNNING, None
                log.info("associated with '%s' (AID %d)", self._bss[1], aid & 0x3FFF)
                if self.stack is not None:
                    self.stack.link_up(self._now)
            else:
                log.debug("association refused, status %d", status)
                self._state, self._next = _AUTHENTICATING, self._now + RETRY_NS
        elif subtype in (_DEAUTH, _DISASSOC):
            log.debug("the firmware ended the link")
            self._state, self._next = _AUTHENTICATING, self._now + RETRY_NS

    def _learn(self, bssid, body):
        if len(body) < 12:
            return
        capabilities = struct.unpack("<H", body[10:12])[0]
        ssid = _information_elements(body[12:]).get(_IE_SSID)
        if ssid is None or self._ssid and ssid.decode("utf-8", "replace") != self._ssid:
            return
        name = ssid.decode("utf-8", "replace")
        if capabilities & _PRIVACY:
            if not self._warned:
                self._warned = True
                log.warning("network '%s' needs a password; only open networks are joined", name)
            return
        log.debug("found '%s' %s", name, _mac(bssid))
        self._bss = (bytes(bssid), name)
        self._state, self._next = _AUTHENTICATING, self._now + _AUTH_DELAY_NS

    def _on_data(self, frame):
        flags = frame[1] << 8
        if self._state != _RUNNING or not flags & _FROM_DS or flags & _TO_DS:
            return
        header = 24 + (2 if frame[0] >> 4 & 8 else 0)
        body = frame[header:]
        if len(body) < 8 or body[:6] != _LLC_SNAP:
            return
        if self.stack is not None:
            self.stack.receive(frame[4:10] + frame[16:22] + body[6:8] + body[8:], self._now)

    # ---- from the host stack

    def send_ethernet(self, frame):
        if self._state != _RUNNING or len(frame) < 14:
            return
        header = struct.pack("<HH", (_DATA << 2) | _TO_DS, 0) + self._bss[0] + self.mac + frame[:6]
        self._queue(header + struct.pack("<H", self._sequence()) + _LLC_SNAP + frame[12:14] + frame[14:])

    # ---- time

    def tick(self, now):
        self._now = now
        due = [frame for release, frame in self._outbox if release <= now]
        self._outbox = [(release, frame) for release, frame in self._outbox if release > now]
        for frame in due:
            if self._deliver is not None:
                self._deliver(frame)
        if self._next is not None and now >= self._next:
            self._next = now + RETRY_NS
            {_SCAN: self._probe, _AUTHENTICATING: self._authenticate, _ASSOCIATING: self._associate}[self._state]()
        if self.stack is not None:
            self.stack.tick(now)

    def next_event_ns(self):
        events = [self._next, min((release for release, _ in self._outbox), default=None)]
        if self.stack is not None:
            events.append(self.stack.next_event_ns())
        return min((e for e in events if e is not None), default=None)

    # ---- sending

    def _queue(self, frame):
        self._outbox.append((self._now + LATENCY_NS, frame))

    def _sequence(self):
        self._seq = (self._seq + 1) & 0xFFF
        return self._seq << 4

    def _management(self, subtype, da, bssid, body):
        header = struct.pack("<HH", subtype << 4, 0) + da + self.mac + bssid
        self._queue(header + struct.pack("<H", self._sequence()) + body)

    def _probe(self):
        ssid = (self._ssid or "").encode("utf-8")
        body = _element(_IE_SSID, ssid) + _element(_IE_RATES, _RATES) + _element(_IE_EXT_RATES, _EXT_RATES)
        self._management(_PROBE_REQ, _BROADCAST, _BROADCAST, body)

    def _authenticate(self):
        self._management(_AUTH, self._bss[0], self._bss[0], struct.pack("<HHH", 0, 1, 0))

    def _associate(self):
        body = struct.pack("<HH", 0x0421, 10) + _element(_IE_SSID, self._bss[1].encode("utf-8"))
        body += _element(_IE_RATES, _RATES) + _element(_IE_EXT_RATES, _EXT_RATES)
        self._management(_ASSOC_REQ, self._bss[0], self._bss[0], body)


def _mac(address):
    return ":".join("%02x" % b for b in address)
