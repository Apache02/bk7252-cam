import struct
import unittest

from host.net import packets as p

MAC_A, MAC_B = bytes.fromhex("02aabbccdd01"), bytes.fromhex("02aabbccdd02")
IP_A, IP_B = bytes([192, 168, 4, 2]), bytes([192, 168, 4, 1])


class ChecksumTest(unittest.TestCase):
    def test_known_ipv4_header_checksum(self):
        header = bytes.fromhex("45000073000040004011 0000 c0a80001 c0a800c7")
        self.assertEqual(p.checksum(header), 0xB861)

    def test_odd_length_is_padded(self):
        self.assertEqual(p.checksum(b"\x01"), 0xFEFF)

    def test_a_correct_packet_sums_to_zero(self):
        header = bytes.fromhex("45000073000040004011 b861 c0a80001 c0a800c7")
        self.assertEqual(p.checksum(header), 0)


class EthernetTest(unittest.TestCase):
    def test_round_trip(self):
        frame = p.ethernet(MAC_B, MAC_A, p.ETH_IP, b"payload")
        self.assertEqual(p.parse_ethernet(frame), (MAC_B, MAC_A, p.ETH_IP, b"payload"))

    def test_a_short_frame_is_not_parsed(self):
        self.assertIsNone(p.parse_ethernet(b"\x00" * 10))


class ArpTest(unittest.TestCase):
    def test_round_trip(self):
        packet = p.arp(p.ARP_REQUEST, MAC_A, IP_A, bytes(6), IP_B)
        self.assertEqual(p.parse_arp(packet), (p.ARP_REQUEST, MAC_A, IP_A, bytes(6), IP_B))

    def test_other_hardware_is_ignored(self):
        packet = bytearray(p.arp(p.ARP_REQUEST, MAC_A, IP_A, bytes(6), IP_B))
        packet[1] = 6
        self.assertIsNone(p.parse_arp(bytes(packet)))


class Ipv4Test(unittest.TestCase):
    def test_round_trip(self):
        packet = p.ipv4(IP_A, IP_B, p.IP_UDP, b"data", ident=7)
        self.assertEqual(p.parse_ipv4(packet), (IP_A, IP_B, p.IP_UDP, b"data"))

    def test_the_header_checksum_is_valid(self):
        packet = p.ipv4(IP_A, IP_B, p.IP_UDP, b"data")
        self.assertEqual(p.checksum(packet[:20]), 0)

    def test_a_corrupt_header_is_dropped(self):
        packet = bytearray(p.ipv4(IP_A, IP_B, p.IP_UDP, b"data"))
        packet[8] ^= 0xFF
        self.assertIsNone(p.parse_ipv4(bytes(packet)))

    def test_fragments_are_dropped(self):
        packet = bytearray(p.ipv4(IP_A, IP_B, p.IP_UDP, b"data"))
        packet[6] |= 0x20  # more fragments
        packet[10:12] = b"\x00\x00"
        packet[10:12] = struct.pack(">H", p.checksum(bytes(packet[:20])))
        self.assertIsNone(p.parse_ipv4(bytes(packet)))

    def test_trailing_padding_is_cut_to_the_total_length(self):
        packet = p.ipv4(IP_A, IP_B, p.IP_UDP, b"data") + b"\x00" * 6
        self.assertEqual(p.parse_ipv4(packet)[3], b"data")


class UdpTest(unittest.TestCase):
    def test_round_trip(self):
        segment = p.udp(IP_A, IP_B, 1234, 53, b"query")
        self.assertEqual(p.parse_udp(IP_A, IP_B, segment), (1234, 53, b"query"))

    def test_a_wrong_checksum_is_dropped(self):
        segment = bytearray(p.udp(IP_A, IP_B, 1234, 53, b"query"))
        segment[-1] ^= 1
        self.assertIsNone(p.parse_udp(IP_A, IP_B, bytes(segment)))

    def test_a_zero_checksum_means_none_was_sent(self):
        segment = bytearray(p.udp(IP_A, IP_B, 1234, 53, b"query"))
        segment[6:8] = b"\x00\x00"
        self.assertEqual(p.parse_udp(IP_A, IP_B, bytes(segment)), (1234, 53, b"query"))


class TcpTest(unittest.TestCase):
    def test_round_trip(self):
        segment = p.tcp(IP_A, IP_B, 4000, 80, seq=10, ack=20, flags=p.TCP_ACK | p.TCP_PSH, window=1000, data=b"GET")
        parsed = p.parse_tcp(IP_A, IP_B, segment)
        self.assertEqual((parsed.sport, parsed.dport, parsed.seq, parsed.ack), (4000, 80, 10, 20))
        self.assertEqual((parsed.flags, parsed.window, parsed.data), (p.TCP_ACK | p.TCP_PSH, 1000, b"GET"))

    def test_the_mss_option_goes_into_a_syn(self):
        segment = p.tcp(IP_A, IP_B, 4000, 80, seq=0, ack=0, flags=p.TCP_SYN, window=1000, mss=1400)
        self.assertEqual(p.parse_tcp(IP_A, IP_B, segment).mss, 1400)

    def test_no_option_means_no_mss(self):
        segment = p.tcp(IP_A, IP_B, 4000, 80, seq=0, ack=0, flags=p.TCP_ACK, window=1000)
        self.assertIsNone(p.parse_tcp(IP_A, IP_B, segment).mss)

    def test_a_wrong_checksum_is_dropped(self):
        segment = bytearray(p.tcp(IP_A, IP_B, 4000, 80, seq=0, ack=0, flags=p.TCP_ACK, window=1, data=b"x"))
        segment[-1] ^= 1
        self.assertIsNone(p.parse_tcp(IP_A, IP_B, bytes(segment)))


class IcmpTest(unittest.TestCase):
    def test_echo_reply_mirrors_the_request(self):
        request = p.icmp_echo(p.ICMP_ECHO_REQUEST, ident=5, seq=9, data=b"ping")
        reply = p.icmp_reply_to(request)
        self.assertEqual(reply[0], p.ICMP_ECHO_REPLY)
        self.assertEqual(reply[4:], request[4:])
        self.assertEqual(p.checksum(reply), 0)

    def test_only_echo_requests_get_a_reply(self):
        self.assertIsNone(p.icmp_reply_to(p.icmp_echo(p.ICMP_ECHO_REPLY, 1, 1, b"")))


class DhcpTest(unittest.TestCase):
    def test_discover_carries_the_message_type_and_mac(self):
        message = p.dhcp_request(p.DHCP_DISCOVER, MAC_A, xid=0x1234)
        parsed = p.parse_dhcp(message)
        self.assertEqual((parsed.op, parsed.xid, parsed.mac, parsed.type), (1, 0x1234, MAC_A, p.DHCP_DISCOVER))

    def test_a_request_names_the_offered_address_and_server(self):
        message = p.dhcp_request(p.DHCP_REQUEST, MAC_A, xid=1, requested=IP_A, server=IP_B)
        options = p.parse_dhcp(message).options
        self.assertEqual(options[p.DHCP_OPT_REQUESTED], IP_A)
        self.assertEqual(options[p.DHCP_OPT_SERVER], IP_B)

    def test_an_offer_is_read(self):
        offer = bytearray(p.dhcp_request(p.DHCP_DISCOVER, MAC_A, xid=9))
        offer[0] = 2
        offer[16:20] = IP_A
        end = offer.rindex(255)  # the end option
        offer[end:] = bytes([p.DHCP_OPT_TYPE, 1, p.DHCP_OFFER, p.DHCP_OPT_SERVER, 4]) + IP_B + bytes([255])
        parsed = p.parse_dhcp(bytes(offer))
        self.assertEqual((parsed.op, parsed.yiaddr, parsed.type), (2, IP_A, p.DHCP_OFFER))
        self.assertEqual(parsed.options[p.DHCP_OPT_SERVER], IP_B)

    def test_garbage_is_not_dhcp(self):
        self.assertIsNone(p.parse_dhcp(b"\x00" * 100))


if __name__ == "__main__":
    unittest.main()
