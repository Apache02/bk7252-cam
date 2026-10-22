import struct
import unittest

from host.wifi_station import LATENCY_NS, RETRY_NS, Station

OUR_MAC = bytes.fromhex("02aabbccdd01")
BSSID = bytes.fromhex("c8478c72838b")
BROADCAST = b"\xff" * 6
MS = 1_000_000
AUTH, ASSOC_REQ, PROBE_REQ = 11, 0, 4


def mgmt(subtype, da, sa, bssid, body):
    return struct.pack("<HH", subtype << 4, 0) + da + sa + bssid + struct.pack("<H", 0) + body


def ie(eid, payload):
    return bytes([eid, len(payload)]) + payload


def beacon(ssid=b"Beken-72838a", privacy=False, channel=6):
    caps = 0x0401 | (0x10 if privacy else 0)
    body = struct.pack("<QHH", 0, 100, caps) + ie(0, ssid) + ie(1, b"\x82\x84") + ie(3, bytes([channel]))
    return mgmt(8, BROADCAST, BSSID, BSSID, body)


def auth_response(status=0, seq=2):
    return mgmt(11, OUR_MAC, BSSID, BSSID, struct.pack("<HHH", 0, seq, status))


def assoc_response(status=0, aid=1):
    return mgmt(1, OUR_MAC, BSSID, BSSID, struct.pack("<HHH", 0x0401, status, aid | 0xC000))


def from_ds_data(da, sa, ethertype, payload):
    header = struct.pack("<HH", (2 << 2) | 0x0200, 0) + da + BSSID + sa + struct.pack("<H", 0)
    return header + b"\xaa\xaa\x03\x00\x00\x00" + struct.pack(">H", ethertype) + payload


def is_mgmt(frame, subtype):
    return frame[0] >> 2 & 3 == 0 and frame[0] >> 4 == subtype


class FakeStack:
    def __init__(self):
        self.calls = []
        self.frames = []

    def link_up(self, now):
        self.calls.append(("link_up", now))

    def receive(self, frame, now):
        self.frames.append(frame)

    def tick(self, now):
        self.calls.append(("tick", now))

    def next_event_ns(self):
        return None


class Rig:
    def __init__(self, **kw):
        self.station = Station(OUR_MAC, **kw)
        self.stack = FakeStack()
        self.station.stack = self.stack
        self.sent = []
        self.station.attach(self.sent.append)
        self.now = 0

    def tick(self, ns):
        self.now = ns
        self.station.tick(ns)

    def advance(self, ns):
        """Tick at `ns`, then once more after the latency, so what that tick queued is out."""
        self.tick(ns)
        self.tick(ns + LATENCY_NS)

    def air_in(self, frame):
        self.station.transmit(frame)

    def join(self):
        self.tick(0)
        self.air_in(beacon())
        self.advance(100 * MS)
        self.air_in(auth_response())
        self.advance(200 * MS)
        self.air_in(assoc_response())
        self.advance(300 * MS)
        self.sent.clear()

    def send_ethernet(self, ethertype=b"\x08\x06", body=b"x"):
        self.station.send_ethernet(BSSID + OUR_MAC + ethertype + body)


class JoinTest(unittest.TestCase):
    def test_a_beacon_starts_authentication(self):
        rig = Rig()
        rig.tick(0)
        rig.air_in(beacon())
        rig.advance(100 * MS)
        auth = [f for f in rig.sent if is_mgmt(f, AUTH)]
        self.assertEqual(len(auth), 1)
        self.assertEqual((auth[0][4:10], auth[0][10:16], auth[0][16:22]), (BSSID, OUR_MAC, BSSID))
        self.assertEqual(struct.unpack("<HHH", auth[0][24:30]), (0, 1, 0))

    def test_an_authentication_reply_starts_association_with_the_ssid(self):
        rig = Rig()
        rig.tick(0)
        rig.air_in(beacon())
        rig.advance(100 * MS)
        rig.sent.clear()
        rig.air_in(auth_response())
        rig.advance(200 * MS)
        assoc = [f for f in rig.sent if is_mgmt(f, ASSOC_REQ)]
        self.assertEqual(len(assoc), 1)
        self.assertIn(ie(0, b"Beken-72838a"), assoc[0][24:])

    def test_an_association_reply_brings_the_link_up(self):
        rig = Rig()
        rig.join()
        self.assertTrue(rig.station.associated)
        self.assertEqual([c for c in rig.stack.calls if c[0] == "link_up"], [("link_up", 200 * MS + LATENCY_NS)])

    def test_a_refused_authentication_is_retried_later(self):
        rig = Rig()
        rig.tick(0)
        rig.air_in(beacon())
        rig.advance(100 * MS)
        rig.air_in(auth_response(status=1))
        rig.sent.clear()
        rig.advance(100 * MS + LATENCY_NS + RETRY_NS)
        self.assertTrue(any(is_mgmt(f, AUTH) for f in rig.sent))
        self.assertFalse(rig.station.associated)

    def test_a_protected_network_is_not_joined(self):
        rig = Rig()
        rig.tick(0)
        with self.assertLogs("emulator.net", level="WARNING"):
            rig.air_in(beacon(privacy=True))
        rig.advance(10_000 * MS)
        self.assertFalse(any(is_mgmt(f, AUTH) for f in rig.sent))

    def test_a_different_ssid_is_ignored_when_one_is_set(self):
        rig = Rig(ssid="Other")
        rig.tick(0)
        rig.air_in(beacon())
        rig.advance(10_000 * MS)
        self.assertFalse(any(is_mgmt(f, AUTH) for f in rig.sent))

    def test_probe_requests_go_out_while_scanning(self):
        rig = Rig()
        rig.tick(0)
        rig.advance(RETRY_NS)
        probes = [f for f in rig.sent if is_mgmt(f, PROBE_REQ)]
        self.assertTrue(probes)
        self.assertEqual(probes[0][4:10], BROADCAST)

    def test_a_deauthentication_means_joining_again(self):
        rig = Rig()
        rig.join()
        rig.air_in(mgmt(12, OUR_MAC, BSSID, BSSID, struct.pack("<H", 3)))
        self.assertFalse(rig.station.associated)
        rig.advance(rig.now + 2 * RETRY_NS)
        self.assertTrue(any(is_mgmt(f, AUTH) for f in rig.sent))

    def test_reset_starts_the_scan_over(self):
        rig = Rig()
        rig.join()
        rig.station.reset()
        self.assertFalse(rig.station.associated)
        rig.air_in(beacon())
        rig.advance(rig.now + 100 * MS)
        self.assertTrue(any(is_mgmt(f, AUTH) for f in rig.sent))

    def test_short_frames_are_ignored(self):
        rig = Rig()
        rig.air_in(b"\x80\x00")
        rig.tick(0)


class DataTest(unittest.TestCase):
    def test_a_frame_from_the_device_becomes_an_ethernet_frame_for_the_stack(self):
        rig = Rig()
        rig.join()
        rig.air_in(from_ds_data(OUR_MAC, BSSID, 0x0800, b"ip-packet"))
        self.assertEqual(rig.stack.frames, [OUR_MAC + BSSID + b"\x08\x00" + b"ip-packet"])

    def test_data_before_the_link_is_up_is_dropped(self):
        rig = Rig()
        rig.tick(0)
        rig.air_in(from_ds_data(OUR_MAC, BSSID, 0x0800, b"ip-packet"))
        self.assertEqual(rig.stack.frames, [])

    def test_an_ethernet_frame_from_the_stack_is_sent_to_the_ap(self):
        rig = Rig()
        rig.join()
        rig.send_ethernet(body=b"arp-body")
        rig.advance(rig.now + LATENCY_NS)
        (frame,) = rig.sent
        fc = struct.unpack("<H", frame[:2])[0]
        self.assertEqual(fc & 0x030C, 0x0108)  # data, to the distribution system
        self.assertEqual((frame[4:10], frame[10:16], frame[16:22]), (BSSID, OUR_MAC, BSSID))
        self.assertEqual(frame[24:], b"\xaa\xaa\x03\x00\x00\x00\x08\x06arp-body")

    def test_frames_reach_the_device_only_after_the_latency(self):
        rig = Rig()
        rig.join()
        start = rig.now
        rig.send_ethernet()
        self.assertEqual(rig.sent, [])
        rig.tick(start + LATENCY_NS - 1)
        self.assertEqual(rig.sent, [])
        rig.tick(start + LATENCY_NS)
        self.assertEqual(len(rig.sent), 1)

    def test_nothing_is_sent_while_not_associated(self):
        rig = Rig()
        rig.tick(0)
        rig.send_ethernet()
        rig.advance(LATENCY_NS * 2)
        self.assertEqual(rig.sent, [])

    def test_sequence_numbers_increase(self):
        rig = Rig()
        rig.join()
        rig.send_ethernet()
        rig.send_ethernet()
        rig.advance(rig.now + LATENCY_NS)
        sequences = [struct.unpack("<H", f[22:24])[0] >> 4 for f in rig.sent]
        self.assertEqual(sequences[1], sequences[0] + 1)


class TimingTest(unittest.TestCase):
    def test_the_stack_is_ticked_and_next_event_covers_the_outbox(self):
        rig = Rig()
        rig.join()
        rig.send_ethernet()
        self.assertEqual(rig.station.next_event_ns(), rig.now + LATENCY_NS)
        rig.tick(rig.now + LATENCY_NS)
        self.assertEqual(rig.stack.calls[-1], ("tick", rig.now))

    def test_next_event_is_the_next_probe_while_scanning(self):
        rig = Rig()
        rig.tick(0)
        self.assertEqual(rig.station.next_event_ns(), RETRY_NS)


if __name__ == "__main__":
    unittest.main()
