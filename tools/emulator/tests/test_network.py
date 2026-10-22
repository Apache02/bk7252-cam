import socket
import struct
import unittest

from host.net import packets as p
from host.net.forward import Publish
from host.network import Network
from tests import test_wifi_station as ts

STATION_MAC = ts.OUR_MAC
AP_MAC = ts.BSSID
AP_IP, STATION_IP = bytes([192, 168, 4, 1]), bytes([192, 168, 4, 2])
MS = 1_000_000


class FakeFirmware:
    """An access point with a DHCP server and a TCP echo service on port 80 ("pong:" + data)."""

    def __init__(self):
        self.network = None
        self.log = []
        self._their_seq = 0

    def attach(self, network):
        self.network = network
        network.attach(self.on_air)

    def beacon(self):
        self.network.transmit(ts.beacon())

    def on_air(self, frame):
        kind, subtype = frame[0] >> 2 & 3, frame[0] >> 4
        if kind == 0 and subtype == 11:
            self.network.transmit(ts.auth_response())
        elif kind == 0 and subtype == 0:
            self.network.transmit(ts.assoc_response())
        elif kind == 2:
            dst, src = frame[16:22], frame[10:16]
            self._on_ethernet(src, frame[30:32], frame[32:])

    def _send(self, ethertype, payload):
        self.network.transmit(ts.from_ds_data(STATION_MAC, AP_MAC, ethertype, payload))

    def _send_ip(self, dst_ip, protocol, payload):
        self._send(p.ETH_IP, p.ipv4(AP_IP, dst_ip, protocol, payload))

    def _on_ethernet(self, src, ethertype, payload):
        kind = struct.unpack(">H", ethertype)[0]
        if kind == p.ETH_ARP:
            op, sender_mac, sender_ip, _, target_ip = p.parse_arp(payload)
            if op == p.ARP_REQUEST and target_ip == AP_IP:
                self._send(p.ETH_ARP, p.arp(p.ARP_REPLY, AP_MAC, AP_IP, sender_mac, sender_ip))
            return
        src_ip, dst_ip, protocol, body = p.parse_ipv4(payload)
        if protocol == p.IP_UDP:
            self._on_dhcp(p.parse_udp(src_ip, dst_ip, body)[2])
        elif protocol == p.IP_TCP:
            self._on_tcp(p.parse_tcp(src_ip, dst_ip, body))

    def _on_dhcp(self, message):
        request = p.parse_dhcp(message)
        kind = {p.DHCP_DISCOVER: p.DHCP_OFFER, p.DHCP_REQUEST: p.DHCP_ACK}[request.type]
        reply = bytearray(p.dhcp_request(p.DHCP_DISCOVER, request.mac, request.xid))
        reply[0] = 2
        reply[16:20] = STATION_IP
        end = reply.rindex(255)
        reply[end:] = bytes([p.DHCP_OPT_TYPE, 1, kind, p.DHCP_OPT_SERVER, 4]) + AP_IP + bytes([255])
        segment = p.udp(AP_IP, STATION_IP, 67, 68, bytes(reply))
        self._send_ip(STATION_IP, p.IP_UDP, segment)
        self.log.append(("dhcp", kind))

    def _on_tcp(self, segment):
        def reply(flags, seq, ack, data=b""):
            self._send_ip(STATION_IP, p.IP_TCP, p.tcp(AP_IP, STATION_IP, 80, segment.sport, seq, ack, flags, 4096, data))

        if segment.flags & p.TCP_SYN:
            self._their_seq = segment.seq + 1
            reply(p.TCP_SYN | p.TCP_ACK, 9000, self._their_seq)
        elif segment.data and segment.seq == self._their_seq:
            self._their_seq += len(segment.data)
            reply(p.TCP_ACK | p.TCP_PSH, 9001, self._their_seq, b"pong:" + segment.data)
            self.log.append(("data", segment.data))
        elif segment.flags & p.TCP_FIN:
            self._their_seq += 1
            reply(p.TCP_FIN | p.TCP_ACK, 9001 + 5 + 4, self._their_seq)


def run(network, firmware, until, limit=3000):
    """Tick the device clock in 1 ms steps until `until()` is true."""
    now = 0
    for _ in range(limit):
        now += MS
        network.tick(now)
        if until():
            return True
    return False


class NetworkTest(unittest.TestCase):
    def make(self, publishes=()):
        network = Network(list(publishes), mac=STATION_MAC, poll_interval=0)
        self.addCleanup(network.close)
        firmware = FakeFirmware()
        firmware.attach(network)
        return network, firmware

    def test_it_is_the_air_the_mac_expects(self):
        network, _ = self.make()
        for name in ("attach", "transmit", "tick", "next_event_ns", "reset", "close"):
            self.assertTrue(callable(getattr(network, name)), name)

    def test_the_station_joins_and_takes_an_address(self):
        network, firmware = self.make()
        firmware.beacon()
        self.assertTrue(run(network, firmware, lambda: ("dhcp", p.DHCP_ACK) in firmware.log))

    def test_a_published_tcp_port_reaches_the_firmware_and_back(self):
        network, firmware = self.make([Publish("127.0.0.1", 0, 80, "tcp")])
        firmware.beacon()
        client = socket.create_connection(("127.0.0.1", network.addresses[0][1]))
        self.addCleanup(client.close)
        client.settimeout(0.01)
        client.sendall(b"ping")
        got = bytearray()

        def done():
            try:
                got.extend(client.recv(100))
            except socket.timeout:
                pass
            return bytes(got) == b"pong:ping"

        self.assertTrue(run(network, firmware, done, limit=6000))

    def test_reset_makes_the_station_scan_again(self):
        network, firmware = self.make()
        firmware.beacon()
        run(network, firmware, lambda: ("dhcp", p.DHCP_ACK) in firmware.log)
        firmware.log.clear()
        network.reset()
        firmware.beacon()
        self.assertTrue(run(network, firmware, lambda: ("dhcp", p.DHCP_ACK) in firmware.log))


if __name__ == "__main__":
    unittest.main()
