"""The host's side of the wireless link: one IPv4 station that talks to the firmware.

The firmware runs an access point with a DHCP server. This stack is the single client of
that network: it takes an address by DHCP, answers ARP and ping, opens TCP connections to
the firmware and exchanges UDP datagrams with it. It works on Ethernet frames; the wifi
station (host/wifi_station.py) carries them over the air. Time is the device clock in
nanoseconds, passed in by the caller.
"""

import logging

from host.net import packets as p
from host.net.tcp import TcpFlow

log = logging.getLogger("emulator.net")

DHCP_RETRY_NS = 3_000_000_000
_BROADCAST_MAC = b"\xff" * 6
_BROADCAST_IP = b"\xff" * 4
_NO_IP = bytes(4)
_FIRST_PORT = 40000
_DISCOVERING, _REQUESTING, _BOUND = "discovering", "requesting", "bound"


class Stack:
    """`output(frame)` takes every Ethernet frame the stack sends."""

    def __init__(self, mac, output):
        self.mac = mac
        self._output = output
        self.reset()

    def reset(self):
        self.ip = None
        self.guest_ip = None
        self.guest_mac = None
        self._dhcp = None
        self._dhcp_deadline = None
        self._xid = 0
        self._offer = None
        self._flows = {}  # our local port -> TcpFlow
        self._udp = {}  # our local port -> handler(guest_port, data)
        self._next_port = _FIRST_PORT
        self._ident = 0
        self._iss = 0

    @property
    def bound(self):
        return self._dhcp == _BOUND

    @property
    def flow_count(self):
        return len(self._flows)

    # ---- DHCP

    def link_up(self, now):
        """The wireless link is up: ask the firmware's DHCP server for an address."""
        self.reset()
        self._discover(now)

    def _discover(self, now):
        self._xid += 1
        self._dhcp = _DISCOVERING
        self._dhcp_deadline = now + DHCP_RETRY_NS
        self._send_dhcp(p.dhcp_request(p.DHCP_DISCOVER, self.mac, self._xid))

    def _send_dhcp(self, message):
        segment = p.udp(_NO_IP, _BROADCAST_IP, p.DHCP_CLIENT_PORT, p.DHCP_SERVER_PORT, message)
        self._output(p.ethernet(_BROADCAST_MAC, self.mac, p.ETH_IP, self._ipv4(_NO_IP, _BROADCAST_IP, p.IP_UDP, segment)))

    def _on_dhcp(self, message, source_mac, now):
        reply = p.parse_dhcp(message)
        if reply is None or reply.op != 2 or reply.xid != self._xid or reply.mac != self.mac:
            return
        server = reply.options.get(p.DHCP_OPT_SERVER)
        if reply.type == p.DHCP_OFFER and self._dhcp == _DISCOVERING and server:
            self._dhcp = _REQUESTING
            self._offer = (reply.yiaddr, server, source_mac)
            self._dhcp_deadline = now + DHCP_RETRY_NS
            self._send_dhcp(p.dhcp_request(p.DHCP_REQUEST, self.mac, self._xid, reply.yiaddr, server))
        elif reply.type == p.DHCP_ACK and self._dhcp == _REQUESTING:
            self.ip, self.guest_ip, self.guest_mac = self._offer
            router = reply.options.get(p.DHCP_OPT_ROUTER)
            self._dhcp = _BOUND
            self._dhcp_deadline = None
            log.debug("got %s from %s", _ip(self.ip), _ip(self.guest_ip))
            if router and len(router) >= 4 and router[:4] != self.guest_ip:
                log.debug("the firmware names router %s; using its DHCP server address", _ip(router[:4]))
        elif reply.type == p.DHCP_NAK:
            log.debug("DHCP request refused, starting again")
            self._discover(now)

    # ---- receive

    def receive(self, frame, now):
        parsed = p.parse_ethernet(frame)
        if parsed is None:
            return
        dst, src, kind, payload = parsed
        if dst != self.mac and dst != _BROADCAST_MAC:
            return
        if kind == p.ETH_ARP:
            self._on_arp(payload)
        elif kind == p.ETH_IP:
            self._on_ip(payload, src, now)

    def _on_arp(self, payload):
        packet = p.parse_arp(payload)
        if packet is None:
            return
        op, sender_mac, sender_ip, _, target_ip = packet
        if sender_ip == self.guest_ip:
            self.guest_mac = sender_mac
        if op == p.ARP_REQUEST and self.ip is not None and target_ip == self.ip:
            self._output(p.ethernet(sender_mac, self.mac, p.ETH_ARP,
                                    p.arp(p.ARP_REPLY, self.mac, self.ip, sender_mac, sender_ip)))

    def _on_ip(self, payload, source_mac, now):
        parsed = p.parse_ipv4(payload)
        if parsed is None:
            return
        src, dst, protocol, body = parsed
        if protocol == p.IP_UDP and self._dhcp not in (None, _BOUND):
            udp = p.parse_udp(src, dst, body)
            if udp is not None and udp[1] == p.DHCP_CLIENT_PORT:
                self._on_dhcp(udp[2], source_mac, now)
                return
        if self.ip is None or dst != self.ip:
            return
        if protocol == p.IP_ICMP:
            reply = p.icmp_reply_to(body)
            if reply is not None:
                self._send_ip(src, p.IP_ICMP, reply)
        elif protocol == p.IP_UDP:
            self._on_udp(src, dst, body)
        elif protocol == p.IP_TCP:
            self._on_tcp(src, dst, body, now)

    def _on_udp(self, src, dst, body):
        udp = p.parse_udp(src, dst, body)
        handler = self._udp.get(udp[1]) if udp else None
        if handler is not None:
            handler(udp[0], udp[2])

    def _on_tcp(self, src, dst, body, now):
        segment = p.parse_tcp(src, dst, body)
        if segment is None:
            return
        flow = self._flows.get(segment.dport)
        if flow is not None:
            flow.receive(segment, now)
        elif not segment.flags & p.TCP_RST:
            self._reset(src, segment)

    def _reset(self, src, segment):
        if segment.flags & p.TCP_ACK:
            seq, ack, flags = segment.ack, 0, p.TCP_RST
        else:
            seq, ack, flags = 0, segment.seq + len(segment.data) + (1 if segment.flags & p.TCP_SYN else 0), p.TCP_RST | p.TCP_ACK
        self._send_ip(src, p.IP_TCP, p.tcp(self.ip, src, segment.dport, segment.sport, seq, ack, flags, 0))

    # ---- the application side

    def allocate_port(self):
        while self._next_port in self._flows or self._next_port in self._udp:
            self._next_port += 1
        port, self._next_port = self._next_port, self._next_port + 1
        return port

    def open_tcp(self, guest_port, deliver, on_peer_closed, on_closed, now):
        """Start a connection to the firmware; None while there is no address yet."""
        if not self.bound:
            return None
        port = self.allocate_port()
        self._iss += 100_000
        flow = TcpFlow(
            self.ip, port, self.guest_ip, guest_port, iss=self._iss,
            emit=lambda segment: self._send_ip(self.guest_ip, p.IP_TCP, segment),
            deliver=deliver, on_peer_closed=on_peer_closed, on_closed=on_closed,
        )
        self._flows[port] = flow
        flow.open(now)
        return flow

    def bind_udp(self, local_port, handler):
        self._udp[local_port] = handler

    def unbind_udp(self, local_port):
        self._udp.pop(local_port, None)

    def send_udp(self, local_port, guest_port, data):
        if not self.bound:
            return False
        return self._send_ip(self.guest_ip, p.IP_UDP, p.udp(self.ip, self.guest_ip, local_port, guest_port, data))

    # ---- time

    def tick(self, now):
        if self._dhcp_deadline is not None and now >= self._dhcp_deadline:
            self._discover(now)
        for port, flow in list(self._flows.items()):
            flow.tick(now)
            if flow.finished:
                del self._flows[port]

    def next_event_ns(self):
        events = [self._dhcp_deadline] + [flow.next_event_ns() for flow in self._flows.values()]
        return min((e for e in events if e is not None), default=None)

    # ---- sending

    def _ipv4(self, src, dst, protocol, payload):
        self._ident = (self._ident + 1) & 0xFFFF
        return p.ipv4(src, dst, protocol, payload, ident=self._ident)

    def _send_ip(self, dst_ip, protocol, payload):
        if self.guest_mac is None or self.ip is None:
            return False
        self._output(p.ethernet(self.guest_mac, self.mac, p.ETH_IP, self._ipv4(self.ip, dst_ip, protocol, payload)))
        return True


def _ip(address):
    return ".".join(str(b) for b in address)
