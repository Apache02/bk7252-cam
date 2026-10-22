"""Build and parse the network packets the emulator's host stack speaks.

MAC addresses and IPv4 addresses are `bytes` (6 and 4). A parse function returns None for a
packet it will not use: too short, wrong checksum, a fragment, an unsupported kind.
"""

import struct
from collections import namedtuple

ETH_IP, ETH_ARP = 0x0800, 0x0806
ARP_REQUEST, ARP_REPLY = 1, 2
IP_ICMP, IP_TCP, IP_UDP = 1, 6, 17
ICMP_ECHO_REPLY, ICMP_ECHO_REQUEST = 0, 8
TCP_FIN, TCP_SYN, TCP_RST, TCP_PSH, TCP_ACK = 0x01, 0x02, 0x04, 0x08, 0x10

DHCP_DISCOVER, DHCP_OFFER, DHCP_REQUEST, DHCP_ACK, DHCP_NAK = 1, 2, 3, 5, 6
DHCP_OPT_SUBNET, DHCP_OPT_ROUTER, DHCP_OPT_DNS = 1, 3, 6
DHCP_OPT_REQUESTED, DHCP_OPT_LEASE, DHCP_OPT_TYPE, DHCP_OPT_SERVER, DHCP_OPT_PARAMS = 50, 51, 53, 54, 55
DHCP_SERVER_PORT, DHCP_CLIENT_PORT = 67, 68

_DHCP_COOKIE = bytes.fromhex("63825363")
_DHCP_FIXED = 236
_BROADCAST_FLAG = 0x8000

Tcp = namedtuple("Tcp", "sport dport seq ack flags window data mss")
Dhcp = namedtuple("Dhcp", "op xid mac yiaddr type options")


def checksum(data):
    """The 16-bit one's complement sum used by IP, ICMP, UDP and TCP."""
    if len(data) % 2:
        data += b"\x00"
    total = sum(struct.unpack(">%dH" % (len(data) // 2), data))
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return ~total & 0xFFFF


def ethernet(dst, src, ethertype, payload):
    return dst + src + struct.pack(">H", ethertype) + payload


def parse_ethernet(frame):
    if len(frame) < 14:
        return None
    return frame[:6], frame[6:12], struct.unpack(">H", frame[12:14])[0], frame[14:]


def arp(op, sender_mac, sender_ip, target_mac, target_ip):
    return struct.pack(">HHBBH", 1, ETH_IP, 6, 4, op) + sender_mac + sender_ip + target_mac + target_ip


def parse_arp(packet):
    if len(packet) < 28:
        return None
    hardware, protocol, hlen, plen, op = struct.unpack(">HHBBH", packet[:8])
    if (hardware, protocol, hlen, plen) != (1, ETH_IP, 6, 4):
        return None
    return op, packet[8:14], packet[14:18], packet[18:24], packet[24:28]


def ipv4(src, dst, protocol, payload, ident=0):
    header = struct.pack(">BBHHHBBH", 0x45, 0, 20 + len(payload), ident & 0xFFFF, 0x4000, 64, protocol, 0) + src + dst
    return header[:10] + struct.pack(">H", checksum(header)) + header[12:] + payload


def parse_ipv4(packet):
    if len(packet) < 20 or packet[0] >> 4 != 4:
        return None
    header_len = (packet[0] & 0xF) * 4
    total = struct.unpack(">H", packet[2:4])[0]
    if header_len < 20 or total < header_len or total > len(packet) or checksum(packet[:header_len]):
        return None
    if struct.unpack(">H", packet[6:8])[0] & 0x3FFF:  # more fragments, or a fragment offset
        return None
    return packet[12:16], packet[16:20], packet[9], packet[header_len:total]


def _pseudo_header(src, dst, protocol, length):
    return src + dst + struct.pack(">BBH", 0, protocol, length)


def udp(src_ip, dst_ip, sport, dport, payload):
    length = 8 + len(payload)
    segment = struct.pack(">HHHH", sport, dport, length, 0) + payload
    total = checksum(_pseudo_header(src_ip, dst_ip, IP_UDP, length) + segment) or 0xFFFF
    return segment[:6] + struct.pack(">H", total) + segment[8:]


def parse_udp(src_ip, dst_ip, segment):
    if len(segment) < 8:
        return None
    sport, dport, length, check = struct.unpack(">HHHH", segment[:8])
    if length < 8 or length > len(segment):
        return None
    segment = segment[:length]
    if check and checksum(_pseudo_header(src_ip, dst_ip, IP_UDP, length) + segment):
        return None
    return sport, dport, segment[8:]


def tcp(src_ip, dst_ip, sport, dport, seq, ack, flags, window, data=b"", mss=None):
    options = struct.pack(">BBH", 2, 4, mss) if mss is not None else b""
    header_len = 20 + len(options)
    segment = struct.pack(
        ">HHIIBBHHH", sport, dport, seq & 0xFFFFFFFF, ack & 0xFFFFFFFF, header_len // 4 << 4, flags, window, 0, 0
    ) + options + data
    total = checksum(_pseudo_header(src_ip, dst_ip, IP_TCP, len(segment)) + segment)
    return segment[:16] + struct.pack(">H", total) + segment[18:]


def parse_tcp(src_ip, dst_ip, segment):
    if len(segment) < 20 or checksum(_pseudo_header(src_ip, dst_ip, IP_TCP, len(segment)) + segment):
        return None
    sport, dport, seq, ack, offset, flags, window, _, _ = struct.unpack(">HHIIBBHHH", segment[:20])
    header_len = (offset >> 4) * 4
    if header_len < 20 or header_len > len(segment):
        return None
    return Tcp(sport, dport, seq, ack, flags, window, segment[header_len:], _tcp_mss(segment[20:header_len]))


def _tcp_mss(options):
    index = 0
    while index < len(options):
        kind = options[index]
        if kind == 0:
            break
        if kind == 1:
            index += 1
            continue
        if index + 1 >= len(options) or options[index + 1] < 2:
            break
        if kind == 2 and options[index + 1] == 4:
            return struct.unpack(">H", options[index + 2:index + 4])[0]
        index += options[index + 1]
    return None


def icmp_echo(kind, ident, seq, data):
    message = struct.pack(">BBHHH", kind, 0, 0, ident, seq) + data
    return message[:2] + struct.pack(">H", checksum(message)) + message[4:]


def icmp_reply_to(message):
    if len(message) < 8 or message[0] != ICMP_ECHO_REQUEST or checksum(message):
        return None
    reply = bytes([ICMP_ECHO_REPLY, 0, 0, 0]) + message[4:]
    return reply[:2] + struct.pack(">H", checksum(reply)) + reply[4:]


def dhcp_request(kind, mac, xid, requested=None, server=None):
    """A DHCP client message (DISCOVER or REQUEST) asking for the address, router and DNS."""
    fixed = struct.pack(">BBBBIHH4s4s4s4s", 1, 1, 6, 0, xid, 0, _BROADCAST_FLAG, bytes(4), bytes(4), bytes(4), bytes(4))
    fixed += mac + bytes(10) + bytes(192)  # chaddr, sname and file
    options = bytes([DHCP_OPT_TYPE, 1, kind])
    if requested is not None:
        options += bytes([DHCP_OPT_REQUESTED, 4]) + requested
    if server is not None:
        options += bytes([DHCP_OPT_SERVER, 4]) + server
    options += bytes([DHCP_OPT_PARAMS, 3, DHCP_OPT_SUBNET, DHCP_OPT_ROUTER, DHCP_OPT_DNS, 255])
    return fixed + _DHCP_COOKIE + options


def parse_dhcp(message):
    if len(message) < _DHCP_FIXED + 4 or message[_DHCP_FIXED:_DHCP_FIXED + 4] != _DHCP_COOKIE:
        return None
    options, index = {}, _DHCP_FIXED + 4
    while index < len(message):
        code = message[index]
        if code == 255:
            break
        if code == 0:
            index += 1
            continue
        if index + 1 >= len(message):
            break
        length = message[index + 1]
        options[code] = bytes(message[index + 2:index + 2 + length])
        index += 2 + length
    kind = options.get(DHCP_OPT_TYPE)
    return Dhcp(
        message[0], struct.unpack(">I", message[4:8])[0], bytes(message[28:34]), bytes(message[16:20]),
        kind[0] if kind else None, options,
    )
