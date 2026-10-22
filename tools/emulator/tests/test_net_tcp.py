import unittest

from host.net import packets as p
from host.net.tcp import RTO_NS, TcpFlow

IP_US, IP_THEM = bytes([192, 168, 4, 2]), bytes([192, 168, 4, 1])
ISS, THEIR_ISS = 1000, 5000
MS = 1_000_000


class Rig:
    def __init__(self):
        self.out = []
        self.received = []
        self.events = []
        self.flow = TcpFlow(
            IP_US, 40000, IP_THEM, 80, iss=ISS,
            emit=lambda segment: self.out.append(p.parse_tcp(IP_US, IP_THEM, segment)),
            deliver=self.received.append,
            on_peer_closed=lambda: self.events.append("peer_closed"),
            on_closed=lambda reset: self.events.append("reset" if reset else "closed"),
        )

    def them(self, flags, seq, ack, data=b"", window=4096, mss=None, now=0):
        self.flow.receive(p.Tcp(80, 40000, seq, ack, flags, window, data, mss), now)

    def establish(self, window=4096, mss=1400):
        self.flow.open(0)
        self.them(p.TCP_SYN | p.TCP_ACK, THEIR_ISS, ISS + 1, window=window, mss=mss)
        self.out.clear()


class HandshakeTest(unittest.TestCase):
    def test_open_sends_a_syn_with_an_mss(self):
        rig = Rig()
        rig.flow.open(0)
        (syn,) = rig.out
        self.assertEqual((syn.flags, syn.seq, syn.mss), (p.TCP_SYN, ISS, 1400))

    def test_a_syn_ack_is_answered_and_the_flow_is_up(self):
        rig = Rig()
        rig.flow.open(0)
        rig.out.clear()
        rig.them(p.TCP_SYN | p.TCP_ACK, THEIR_ISS, ISS + 1, mss=536)
        (ack,) = rig.out
        self.assertEqual((ack.flags, ack.seq, ack.ack), (p.TCP_ACK, ISS + 1, THEIR_ISS + 1))
        self.assertTrue(rig.flow.established)

    def test_a_wrong_ack_in_the_syn_ack_resets(self):
        rig = Rig()
        rig.flow.open(0)
        rig.out.clear()
        rig.them(p.TCP_SYN | p.TCP_ACK, THEIR_ISS, 12345)
        self.assertEqual(rig.out[0].flags & p.TCP_RST, p.TCP_RST)
        self.assertFalse(rig.flow.established)

    def test_the_syn_is_resent_and_then_given_up(self):
        rig = Rig()
        rig.flow.open(0)
        for attempt in range(1, 6):
            rig.flow.tick(attempt * RTO_NS * 2 ** attempt)
        self.assertEqual(len(rig.out), 6)
        self.assertEqual(rig.events, [])
        rig.flow.tick(10_000 * RTO_NS)
        self.assertEqual(rig.events, ["reset"])
        self.assertTrue(rig.flow.finished)

    def test_a_reset_in_reply_to_the_syn_closes(self):
        rig = Rig()
        rig.flow.open(0)
        rig.them(p.TCP_RST | p.TCP_ACK, 0, ISS + 1)
        self.assertEqual(rig.events, ["reset"])


class SendTest(unittest.TestCase):
    def test_data_is_cut_into_segments_of_the_mss(self):
        rig = Rig()
        rig.establish(window=65535)
        rig.flow.write(b"x" * 3000, 0)
        self.assertEqual([len(s.data) for s in rig.out], [1400, 1400, 200])
        self.assertEqual([s.seq for s in rig.out], [ISS + 1, ISS + 1401, ISS + 2801])

    def test_the_peers_mss_caps_the_segment_size(self):
        rig = Rig()
        rig.establish(window=65535, mss=536)
        rig.flow.write(b"x" * 1000, 0)
        self.assertEqual([len(s.data) for s in rig.out], [536, 464])

    def test_the_peers_window_limits_what_is_in_flight(self):
        rig = Rig()
        rig.establish(window=1000)
        rig.flow.write(b"x" * 3000, 0)
        self.assertEqual(sum(len(s.data) for s in rig.out), 1000)
        rig.out.clear()
        rig.them(p.TCP_ACK, THEIR_ISS + 1, ISS + 1 + 1000, window=1000)
        self.assertEqual(sum(len(s.data) for s in rig.out), 1000)

    def test_data_written_before_the_flow_is_up_goes_out_afterwards(self):
        rig = Rig()
        rig.flow.open(0)
        rig.flow.write(b"hello", 0)
        rig.out.clear()
        rig.them(p.TCP_SYN | p.TCP_ACK, THEIR_ISS, ISS + 1)
        data = [s for s in rig.out if s.data]
        self.assertEqual([s.data for s in data], [b"hello"])

    def test_unacked_data_is_resent_after_the_timeout(self):
        rig = Rig()
        rig.establish()
        rig.flow.write(b"hello", 0)
        rig.out.clear()
        rig.flow.tick(RTO_NS - 1)
        self.assertEqual(rig.out, [])
        rig.flow.tick(RTO_NS)
        self.assertEqual([s.data for s in rig.out], [b"hello"])
        self.assertEqual(rig.out[0].seq, ISS + 1)

    def test_acked_data_is_not_resent(self):
        rig = Rig()
        rig.establish()
        rig.flow.write(b"hello", 0)
        rig.them(p.TCP_ACK, THEIR_ISS + 1, ISS + 1 + 5)
        rig.out.clear()
        rig.flow.tick(10 * RTO_NS)
        self.assertEqual(rig.out, [])

    def test_can_write_turns_false_when_the_buffer_is_full(self):
        rig = Rig()
        rig.establish(window=0)
        self.assertTrue(rig.flow.can_write())
        rig.flow.write(b"x" * 100_000, 0)
        self.assertFalse(rig.flow.can_write())

    def test_the_next_event_is_the_retransmission_time(self):
        rig = Rig()
        rig.establish()
        self.assertIsNone(rig.flow.next_event_ns())
        rig.flow.write(b"hello", 5 * MS)
        self.assertEqual(rig.flow.next_event_ns(), 5 * MS + RTO_NS)


class ReceiveTest(unittest.TestCase):
    def test_in_order_data_is_delivered_and_acked(self):
        rig = Rig()
        rig.establish()
        rig.them(p.TCP_ACK | p.TCP_PSH, THEIR_ISS + 1, ISS + 1, data=b"HTTP/1.0 200")
        self.assertEqual(rig.received, [b"HTTP/1.0 200"])
        self.assertEqual(rig.out[-1].ack, THEIR_ISS + 1 + 12)

    def test_out_of_order_data_is_dropped_and_the_expected_byte_is_acked_again(self):
        rig = Rig()
        rig.establish()
        rig.them(p.TCP_ACK, THEIR_ISS + 100, ISS + 1, data=b"late")
        self.assertEqual(rig.received, [])
        self.assertEqual(rig.out[-1].ack, THEIR_ISS + 1)

    def test_a_retransmitted_segment_is_acked_not_delivered_twice(self):
        rig = Rig()
        rig.establish()
        rig.them(p.TCP_ACK, THEIR_ISS + 1, ISS + 1, data=b"abc")
        rig.them(p.TCP_ACK, THEIR_ISS + 1, ISS + 1, data=b"abc")
        self.assertEqual(rig.received, [b"abc"])
        self.assertEqual(rig.out[-1].ack, THEIR_ISS + 4)

    def test_a_segment_overlapping_old_data_delivers_only_the_new_bytes(self):
        rig = Rig()
        rig.establish()
        rig.them(p.TCP_ACK, THEIR_ISS + 1, ISS + 1, data=b"abc")
        rig.them(p.TCP_ACK, THEIR_ISS + 2, ISS + 1, data=b"bcde")
        self.assertEqual(b"".join(rig.received), b"abcde")


class CloseTest(unittest.TestCase):
    def test_a_fin_from_the_peer_is_acked_and_reported(self):
        rig = Rig()
        rig.establish()
        rig.them(p.TCP_FIN | p.TCP_ACK, THEIR_ISS + 1, ISS + 1)
        self.assertEqual(rig.events, ["peer_closed"])
        self.assertEqual(rig.out[-1].ack, THEIR_ISS + 2)

    def test_close_sends_a_fin_after_the_data(self):
        rig = Rig()
        rig.establish()
        rig.flow.write(b"bye", 0)
        rig.flow.close(0)
        self.assertEqual([(s.data, s.flags & p.TCP_FIN) for s in rig.out], [(b"bye", 0), (b"", p.TCP_FIN)])

    def test_both_sides_closed_and_acked_finishes_the_flow(self):
        rig = Rig()
        rig.establish()
        rig.flow.close(0)
        rig.them(p.TCP_ACK, THEIR_ISS + 1, ISS + 2)
        rig.them(p.TCP_FIN | p.TCP_ACK, THEIR_ISS + 1, ISS + 2)
        self.assertEqual(rig.events, ["peer_closed", "closed"])
        self.assertTrue(rig.flow.finished)

    def test_the_peer_closing_first_then_us_finishes_too(self):
        rig = Rig()
        rig.establish()
        rig.them(p.TCP_FIN | p.TCP_ACK, THEIR_ISS + 1, ISS + 1)
        rig.flow.close(0)
        rig.them(p.TCP_ACK, THEIR_ISS + 2, ISS + 2)
        self.assertEqual(rig.events, ["peer_closed", "closed"])

    def test_a_reset_ends_the_flow_at_once(self):
        rig = Rig()
        rig.establish()
        rig.them(p.TCP_RST, THEIR_ISS + 1, 0)
        self.assertEqual(rig.events, ["reset"])
        self.assertTrue(rig.flow.finished)

    def test_abort_sends_a_reset(self):
        rig = Rig()
        rig.establish()
        rig.flow.abort()
        self.assertTrue(rig.out[-1].flags & p.TCP_RST)
        self.assertTrue(rig.flow.finished)

    def test_a_finished_flow_ignores_segments_and_ticks(self):
        rig = Rig()
        rig.establish()
        rig.flow.abort()
        rig.out.clear()
        rig.them(p.TCP_ACK, THEIR_ISS + 1, ISS + 1, data=b"x")
        rig.flow.tick(100 * RTO_NS)
        self.assertEqual((rig.out, rig.received), ([], []))


if __name__ == "__main__":
    unittest.main()
