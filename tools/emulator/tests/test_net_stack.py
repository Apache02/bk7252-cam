import struct
import unittest

from host.net import packets as p
from host.net.stack import DHCP_RETRY_NS, Stack

OUR_MAC = bytes.fromhex("02aabbccdd01")
GUEST_MAC = bytes.fromhex("c8478c72838b")
OUR_IP, GUEST_IP = bytes([192, 168, 4, 2]), bytes([192, 168, 4, 1])
BROADCAST = b"\xff" * 6
S = 1_000_000_000


def eth_ip(src_mac, dst_mac, src_ip, dst_ip, protocol, payload):
    return p.ethernet(dst_mac, src_mac, p.ETH_IP, p.ipv4(src_ip, dst_ip, protocol, payload))


def parse(frame):
    dst, src, kind, payload = p.parse_ethernet(frame)
    return dst, src, kind, payload


def ip_of(frame):
    _, _, kind, payload = parse(frame)
    return p.parse_ipv4(payload) if kind == p.ETH_IP else None


class Rig:
    def __init__(self):
        self.out = []
        self.stack = Stack(OUR_MAC, output=self.out.append)

    def dhcp_from_guest(self, kind, xid, dst_mac=None):
        message = bytearray(p.dhcp_request(p.DHCP_DISCOVER, OUR_MAC, xid))
        message[0] = 2
        message[16:20] = OUR_IP
        end = message.rindex(255)
        message[end:] = (
            bytes([p.DHCP_OPT_TYPE, 1, kind, p.DHCP_OPT_SERVER, 4]) + GUEST_IP
            + bytes([p.DHCP_OPT_SUBNET, 4, 255, 255, 255, 0]) + bytes([255])
        )
        segment = p.udp(GUEST_IP, OUR_IP, 67, 68, bytes(message))
        return eth_ip(GUEST_MAC, dst_mac or OUR_MAC, GUEST_IP, OUR_IP, p.IP_UDP, segment)

    def bind(self):
        self.stack.link_up(0)
        xid = p.parse_dhcp(p.parse_udp(*ip_of(self.out[-1])[:2], ip_of(self.out[-1])[3])[2]).xid
        self.stack.receive(self.dhcp_from_guest(p.DHCP_OFFER, xid), 0)
        self.stack.receive(self.dhcp_from_guest(p.DHCP_ACK, xid), 0)
        self.out.clear()


class DhcpTest(unittest.TestCase):
    def test_link_up_broadcasts_a_discover(self):
        rig = Rig()
        rig.stack.link_up(0)
        (frame,) = rig.out
        dst, src, kind, _ = parse(frame)
        self.assertEqual((dst, src, kind), (BROADCAST, OUR_MAC, p.ETH_IP))
        src_ip, dst_ip, protocol, payload = ip_of(frame)
        self.assertEqual((src_ip, dst_ip, protocol), (bytes(4), b"\xff" * 4, p.IP_UDP))
        sport, dport, data = p.parse_udp(src_ip, dst_ip, payload)
        self.assertEqual((sport, dport), (68, 67))
        self.assertEqual(p.parse_dhcp(data).type, p.DHCP_DISCOVER)

    def test_an_offer_is_answered_with_a_request_for_that_address_and_server(self):
        rig = Rig()
        rig.stack.link_up(0)
        xid = p.parse_dhcp(p.parse_udp(*ip_of(rig.out[0])[:2], ip_of(rig.out[0])[3])[2]).xid
        rig.out.clear()
        rig.stack.receive(rig.dhcp_from_guest(p.DHCP_OFFER, xid), 0)
        (frame,) = rig.out
        src_ip, dst_ip, _, payload = ip_of(frame)
        request = p.parse_dhcp(p.parse_udp(src_ip, dst_ip, payload)[2])
        self.assertEqual(request.type, p.DHCP_REQUEST)
        self.assertEqual(request.options[p.DHCP_OPT_REQUESTED], OUR_IP)
        self.assertEqual(request.options[p.DHCP_OPT_SERVER], GUEST_IP)

    def test_an_ack_binds_the_address(self):
        rig = Rig()
        rig.bind()
        self.assertEqual((rig.stack.ip, rig.stack.guest_ip), (OUR_IP, GUEST_IP))
        self.assertTrue(rig.stack.bound)

    def test_a_reply_for_another_transaction_is_ignored(self):
        rig = Rig()
        rig.stack.link_up(0)
        rig.stack.receive(rig.dhcp_from_guest(p.DHCP_OFFER, 0xDEAD), 0)
        self.assertEqual(len(rig.out), 1)

    def test_a_nak_starts_again(self):
        rig = Rig()
        rig.stack.link_up(0)
        xid = p.parse_dhcp(p.parse_udp(*ip_of(rig.out[0])[:2], ip_of(rig.out[0])[3])[2]).xid
        rig.stack.receive(rig.dhcp_from_guest(p.DHCP_OFFER, xid), 0)
        rig.out.clear()
        rig.stack.receive(rig.dhcp_from_guest(p.DHCP_NAK, xid), 0)
        self.assertFalse(rig.stack.bound)
        src_ip, dst_ip, _, payload = ip_of(rig.out[-1])
        self.assertEqual(p.parse_dhcp(p.parse_udp(src_ip, dst_ip, payload)[2]).type, p.DHCP_DISCOVER)

    def test_the_discover_is_repeated_while_nobody_answers(self):
        rig = Rig()
        rig.stack.link_up(0)
        rig.stack.tick(DHCP_RETRY_NS - 1)
        self.assertEqual(len(rig.out), 1)
        rig.stack.tick(DHCP_RETRY_NS)
        self.assertEqual(len(rig.out), 2)
        self.assertEqual(rig.stack.next_event_ns(), 2 * DHCP_RETRY_NS)

    def test_no_retry_once_bound(self):
        rig = Rig()
        rig.bind()
        rig.stack.tick(100 * S)
        self.assertEqual(rig.out, [])
        self.assertIsNone(rig.stack.next_event_ns())


class ArpIcmpTest(unittest.TestCase):
    def test_an_arp_request_for_our_address_is_answered(self):
        rig = Rig()
        rig.bind()
        request = p.ethernet(BROADCAST, GUEST_MAC, p.ETH_ARP, p.arp(p.ARP_REQUEST, GUEST_MAC, GUEST_IP, bytes(6), OUR_IP))
        rig.stack.receive(request, 0)
        (frame,) = rig.out
        dst, src, kind, payload = parse(frame)
        self.assertEqual((dst, src, kind), (GUEST_MAC, OUR_MAC, p.ETH_ARP))
        self.assertEqual(p.parse_arp(payload), (p.ARP_REPLY, OUR_MAC, OUR_IP, GUEST_MAC, GUEST_IP))

    def test_an_arp_request_for_someone_else_is_ignored(self):
        rig = Rig()
        rig.bind()
        other = bytes([192, 168, 4, 99])
        rig.stack.receive(p.ethernet(BROADCAST, GUEST_MAC, p.ETH_ARP, p.arp(p.ARP_REQUEST, GUEST_MAC, GUEST_IP, bytes(6), other)), 0)
        self.assertEqual(rig.out, [])

    def test_a_ping_is_answered(self):
        rig = Rig()
        rig.bind()
        echo = p.icmp_echo(p.ICMP_ECHO_REQUEST, 1, 2, b"hi")
        rig.stack.receive(eth_ip(GUEST_MAC, OUR_MAC, GUEST_IP, OUR_IP, p.IP_ICMP, echo), 0)
        src_ip, dst_ip, protocol, payload = ip_of(rig.out[0])
        self.assertEqual((src_ip, dst_ip, protocol, payload[0]), (OUR_IP, GUEST_IP, p.IP_ICMP, p.ICMP_ECHO_REPLY))

    def test_traffic_for_another_address_is_ignored(self):
        rig = Rig()
        rig.bind()
        echo = p.icmp_echo(p.ICMP_ECHO_REQUEST, 1, 2, b"hi")
        rig.stack.receive(eth_ip(GUEST_MAC, OUR_MAC, GUEST_IP, bytes([10, 0, 0, 9]), p.IP_ICMP, echo), 0)
        self.assertEqual(rig.out, [])


class UdpTest(unittest.TestCase):
    def test_a_datagram_goes_to_the_guest(self):
        rig = Rig()
        rig.bind()
        self.assertTrue(rig.stack.send_udp(40001, 5000, b"hello"))
        dst, _, _, _ = parse(rig.out[0])
        src_ip, dst_ip, protocol, payload = ip_of(rig.out[0])
        self.assertEqual((dst, src_ip, dst_ip, protocol), (GUEST_MAC, OUR_IP, GUEST_IP, p.IP_UDP))
        self.assertEqual(p.parse_udp(src_ip, dst_ip, payload), (40001, 5000, b"hello"))

    def test_nothing_is_sent_before_an_address_is_bound(self):
        rig = Rig()
        self.assertFalse(rig.stack.send_udp(40001, 5000, b"hello"))
        self.assertEqual(rig.out, [])

    def test_a_reply_goes_to_the_handler_bound_to_the_port(self):
        rig = Rig()
        rig.bind()
        got = []
        rig.stack.bind_udp(40001, lambda guest_port, data: got.append((guest_port, data)))
        segment = p.udp(GUEST_IP, OUR_IP, 5000, 40001, b"pong")
        rig.stack.receive(eth_ip(GUEST_MAC, OUR_MAC, GUEST_IP, OUR_IP, p.IP_UDP, segment), 0)
        self.assertEqual(got, [(5000, b"pong")])

    def test_a_datagram_for_an_unbound_port_is_dropped(self):
        rig = Rig()
        rig.bind()
        segment = p.udp(GUEST_IP, OUR_IP, 5000, 40009, b"pong")
        rig.stack.receive(eth_ip(GUEST_MAC, OUR_MAC, GUEST_IP, OUR_IP, p.IP_UDP, segment), 0)
        self.assertEqual(rig.out, [])

    def test_unbinding_stops_delivery(self):
        rig = Rig()
        rig.bind()
        got = []
        rig.stack.bind_udp(40001, lambda *args: got.append(args))
        rig.stack.unbind_udp(40001)
        segment = p.udp(GUEST_IP, OUR_IP, 5000, 40001, b"x")
        rig.stack.receive(eth_ip(GUEST_MAC, OUR_MAC, GUEST_IP, OUR_IP, p.IP_UDP, segment), 0)
        self.assertEqual(got, [])


class TcpTest(unittest.TestCase):
    def open_flow(self, rig):
        events = {"data": [], "closed": []}
        flow = rig.stack.open_tcp(
            80, deliver=events["data"].append, on_peer_closed=lambda: None, on_closed=events["closed"].append, now=0,
        )
        return flow, events

    def guest_segment(self, rig, sport, dport, **kw):
        segment = p.tcp(GUEST_IP, OUR_IP, sport, dport, window=4096, **kw)
        return eth_ip(GUEST_MAC, OUR_MAC, GUEST_IP, OUR_IP, p.IP_TCP, segment)

    def sent_segment(self, frame):
        src_ip, dst_ip, _, payload = ip_of(frame)
        return p.parse_tcp(src_ip, dst_ip, payload)

    def test_no_flow_before_an_address_is_bound(self):
        rig = Rig()
        flow, _ = self.open_flow(rig)
        self.assertIsNone(flow)

    def test_open_sends_a_syn_to_the_guest(self):
        rig = Rig()
        rig.bind()
        flow, _ = self.open_flow(rig)
        self.assertIsNotNone(flow)
        syn = self.sent_segment(rig.out[0])
        self.assertEqual((syn.dport, syn.flags), (80, p.TCP_SYN))
        self.assertEqual(parse(rig.out[0])[0], GUEST_MAC)

    def test_segments_from_the_guest_reach_the_flow(self):
        rig = Rig()
        rig.bind()
        flow, events = self.open_flow(rig)
        syn = self.sent_segment(rig.out[0])
        rig.stack.receive(self.guest_segment(rig, 80, syn.sport, seq=7000, ack=syn.seq + 1, flags=p.TCP_SYN | p.TCP_ACK), 0)
        self.assertTrue(flow.established)
        rig.stack.receive(self.guest_segment(rig, 80, syn.sport, seq=7001, ack=syn.seq + 1, flags=p.TCP_ACK, data=b"hi"), 0)
        self.assertEqual(events["data"], [b"hi"])

    def test_each_flow_gets_its_own_local_port(self):
        rig = Rig()
        rig.bind()
        self.open_flow(rig)
        self.open_flow(rig)
        ports = {self.sent_segment(frame).sport for frame in rig.out}
        self.assertEqual(len(ports), 2)

    def test_a_segment_for_no_flow_is_answered_with_a_reset(self):
        rig = Rig()
        rig.bind()
        rig.stack.receive(self.guest_segment(rig, 80, 45555, seq=1, ack=1, flags=p.TCP_ACK, data=b"x"), 0)
        reset = self.sent_segment(rig.out[0])
        self.assertTrue(reset.flags & p.TCP_RST)

    def test_a_finished_flow_is_forgotten(self):
        rig = Rig()
        rig.bind()
        flow, _ = self.open_flow(rig)
        syn = self.sent_segment(rig.out[0])
        flow.abort()
        rig.stack.tick(0)
        self.assertEqual(rig.stack.flow_count, 0)
        rig.stack.receive(self.guest_segment(rig, 80, syn.sport, seq=1, ack=1, flags=p.TCP_ACK), 0)

    def test_the_stack_ticks_its_flows_and_reports_the_next_event(self):
        rig = Rig()
        rig.bind()
        self.open_flow(rig)
        self.assertEqual(rig.stack.next_event_ns(), S)
        rig.out.clear()
        rig.stack.tick(S)
        self.assertEqual(self.sent_segment(rig.out[0]).flags, p.TCP_SYN)  # resent


if __name__ == "__main__":
    unittest.main()
