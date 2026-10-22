import struct
import unittest

from modules.wifi.mac_frames import FrameEngine

BASE = 0x00900000
THD_PATTERN, TBD_PATTERN, RHD_PATTERN, RBD_PATTERN = 0xCAFEBABE, 0xCAFEFADE, 0xBAADF00D, 0xC0DEDBAD
INTERRUPT_EN_TX = 1 << 8
DESC_DONE, FRAME_OK = 1 << 31, 1 << 23
RX_DONE, RX_SUCCESS, RX_LAST = 0x4000, 0x2000, 0x0001
BCN, AC0 = 4, 0


class Memory:
    def __init__(self):
        self.data = bytearray(0x10000)

    def read(self, address, size):
        return bytes(self.data[address - BASE:address - BASE + size])

    def write(self, address, data):
        self.data[address - BASE:address - BASE + len(data)] = data

    def word(self, address):
        return struct.unpack("<I", self.read(address, 4))[0]


FRAME = bytes.fromhex("8000 0000 ffffffffffff 02aabbccddee 02aabbccddee 0000") + b"beacon-body"


class Rig:
    def __init__(self):
        self.memory = Memory()
        self.sent = []
        self.raised = []
        self.engine = FrameEngine(self.memory, self.sent.append, self.raised.append)

    def put_tx(self, thd, tbd, frame=FRAME, next_thd=0, flags=INTERRUPT_EN_TX):
        data = BASE + 0x800 + (tbd - BASE)
        self.memory.write(data, frame)
        self.memory.write(tbd, struct.pack("<5I", TBD_PATTERN, 0, data, data + len(frame) - 1, 0))
        words = [THD_PATTERN, next_thd, 0, tbd, 0, 0, len(frame) + 4, 0, 0, 0, 0, 0, 0, 0, flags, 0, 0]
        self.memory.write(thd, struct.pack("<17I", *words))

    def put_rx(self, rhd, rbd, next_rhd=0, size=256):
        buf = BASE + 0xC00 + (rbd - BASE)
        self.memory.write(rbd, struct.pack("<5I", RBD_PATTERN, 0, buf, buf + size - 1, 0))
        words = [RHD_PATTERN, next_rhd, rbd, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        self.memory.write(rhd, struct.pack("<17I", *words))
        return buf


class TxTest(unittest.TestCase):
    def test_a_queued_frame_is_handed_to_the_air_with_its_fcs_room_removed(self):
        rig = Rig()
        rig.put_tx(BASE, BASE + 0x100)
        rig.engine.transmit(BCN, BASE, now_ns=0)
        self.assertEqual(rig.sent, [FRAME])

    def test_a_descriptor_chain_is_sent_in_order(self):
        rig = Rig()
        rig.put_tx(BASE, BASE + 0x100, frame=FRAME + b"1", next_thd=BASE + 0x200)
        rig.put_tx(BASE + 0x200, BASE + 0x300, frame=FRAME + b"2")
        rig.engine.transmit(AC0, BASE, now_ns=0)
        self.assertEqual(rig.sent, [FRAME + b"1", FRAME + b"2"])

    def test_the_buffer_descriptor_is_marked_done(self):
        rig = Rig()
        rig.put_tx(BASE, BASE + 0x100)
        rig.engine.transmit(AC0, BASE, now_ns=0)
        self.assertTrue(rig.memory.word(BASE + 0x100 + 16) & (1 << 31))

    def test_completion_comes_later_and_raises_the_queue_trigger(self):
        rig = Rig()
        rig.put_tx(BASE, BASE + 0x100)
        rig.engine.transmit(AC0, BASE, now_ns=1000)
        rig.engine.complete(now_ns=1000)
        self.assertEqual(rig.raised, [])
        self.assertIsNotNone(rig.engine.next_event_ns())
        rig.engine.complete(now_ns=1000 + 150_000)
        status = rig.memory.word(BASE + 60)
        self.assertTrue(status & DESC_DONE and status & FRAME_OK)
        self.assertEqual(rig.raised, [1 << 6])  # AC0 trigger
        self.assertIsNone(rig.engine.next_event_ns())

    def test_no_trigger_unless_the_descriptor_asks_for_one(self):
        rig = Rig()
        rig.put_tx(BASE, BASE + 0x100, flags=0)
        rig.engine.transmit(AC0, BASE, now_ns=0)
        rig.engine.complete(now_ns=1_000_000)
        self.assertEqual(rig.raised, [])
        self.assertTrue(rig.memory.word(BASE + 60) & DESC_DONE)

    def test_the_beacon_queue_has_its_own_trigger_bit(self):
        rig = Rig()
        rig.put_tx(BASE, BASE + 0x100)
        rig.engine.transmit(BCN, BASE, now_ns=0)
        rig.engine.complete(now_ns=1_000_000)
        self.assertEqual(rig.raised, [1 << 10])

    def test_a_bad_pattern_stops_the_queue(self):
        rig = Rig()
        rig.put_tx(BASE, BASE + 0x100)
        rig.memory.write(BASE, struct.pack("<I", 0))
        with self.assertLogs("emulator.wifi.frames", level="WARNING"):
            rig.engine.transmit(AC0, BASE, now_ns=0)
        self.assertEqual(rig.sent, [])

    def test_a_descriptor_without_data_waits_for_the_next_kick(self):
        rig = Rig()
        rig.put_tx(BASE, BASE + 0x100)
        rig.memory.write(BASE + 12, struct.pack("<I", 0))  # no payload buffer yet
        rig.engine.transmit(AC0, BASE, now_ns=0)
        self.assertEqual(rig.sent, [])

    def test_a_descriptor_in_unmapped_memory_is_ignored(self):
        rig = Rig()

        def bad_read(address, size):
            raise IOError("unmapped")

        rig.memory.read = bad_read
        with self.assertLogs("emulator.wifi.frames", level="WARNING"):
            rig.engine.transmit(AC0, BASE, now_ns=0)
        self.assertEqual(rig.sent, [])


class RxTest(unittest.TestCase):
    def rig_with_rx(self):
        rig = Rig()
        buf = rig.put_rx(BASE + 0x400, BASE + 0x500)
        rig.engine.set_rx_heads(header=BASE + 0x400, payload=BASE + 0x500)
        return rig, buf

    def test_a_frame_lands_in_the_payload_buffer(self):
        rig, buf = self.rig_with_rx()
        rig.engine.receive(FRAME, tsf=77, rssi=-40, key_index=None)
        self.assertEqual(rig.memory.read(buf, len(FRAME)), FRAME)

    def test_the_header_descriptor_reports_length_and_status(self):
        rig, buf = self.rig_with_rx()
        rig.engine.receive(FRAME, tsf=0x1_0000_0005, rssi=-40, key_index=None)
        rhd = BASE + 0x400
        self.assertEqual(rig.memory.word(rhd + 28) & 0xFFFF, len(FRAME))
        self.assertEqual(rig.memory.word(rhd + 32), 5)
        self.assertEqual(rig.memory.word(rhd + 36), 1)
        status = rig.memory.word(rhd + 64)
        self.assertTrue(status & RX_DONE and status & RX_SUCCESS)
        self.assertEqual(rig.memory.word(rhd + 8), BASE + 0x500)  # first payload descriptor
        self.assertTrue(rig.memory.word(BASE + 0x500 + 16) & RX_LAST)

    def test_receiving_raises_the_rx_triggers(self):
        rig, _ = self.rig_with_rx()
        rig.engine.receive(FRAME, tsf=0, rssi=-40, key_index=None)
        self.assertEqual(rig.raised, [1 << 16 | 1 << 17 | 1 << 19])

    def test_a_known_sender_reports_its_key_index(self):
        rig, _ = self.rig_with_rx()
        rig.engine.receive(FRAME, tsf=0, rssi=-40, key_index=30)
        status = rig.memory.word(BASE + 0x400 + 64)
        self.assertTrue(status & 0x02000000)
        self.assertEqual(status >> 15 & 0x3FF, 30)

    def test_a_group_addressed_frame_is_flagged(self):
        rig, _ = self.rig_with_rx()
        rig.engine.receive(FRAME, tsf=0, rssi=-40, key_index=None)  # destination ff:...
        self.assertTrue(rig.memory.word(BASE + 0x400 + 64) & 0x0400)

    def test_without_descriptors_the_frame_waits_for_the_next_kick(self):
        rig = Rig()
        rig.engine.receive(FRAME, tsf=0, rssi=-40, key_index=None)
        self.assertEqual(rig.raised, [])
        buf = rig.put_rx(BASE + 0x400, BASE + 0x500)
        rig.engine.set_rx_heads(header=BASE + 0x400, payload=BASE + 0x500)
        self.assertEqual(rig.memory.read(buf, len(FRAME)), FRAME)
        self.assertEqual(len(rig.raised), 1)

    def test_a_frame_longer_than_one_buffer_spans_descriptors(self):
        rig = Rig()
        long_frame = FRAME + bytes(range(100))
        buf = rig.put_rx(BASE + 0x400, BASE + 0x500, size=64)
        second = BASE + 0x600
        buf2 = BASE + 0xD00
        rig.memory.write(BASE + 0x500 + 4, struct.pack("<I", second))
        rig.memory.write(second, struct.pack("<5I", RBD_PATTERN, 0, buf2, buf2 + 127, 0))
        rig.engine.set_rx_heads(header=BASE + 0x400, payload=BASE + 0x500)
        rig.engine.receive(long_frame, tsf=0, rssi=-40, key_index=None)
        self.assertEqual(rig.memory.read(buf, 64) + rig.memory.read(buf2, len(long_frame) - 64), long_frame)
        self.assertFalse(rig.memory.word(BASE + 0x500 + 16) & RX_LAST)
        self.assertTrue(rig.memory.word(second + 16) & RX_LAST)

    def test_the_next_frame_uses_the_next_header_descriptor(self):
        rig = Rig()
        rig.put_rx(BASE + 0x400, BASE + 0x500, next_rhd=BASE + 0x480)
        rig.put_rx(BASE + 0x480, BASE + 0x540)
        rig.memory.write(BASE + 0x500 + 4, struct.pack("<I", BASE + 0x540))
        rig.engine.set_rx_heads(header=BASE + 0x400, payload=BASE + 0x500)
        rig.engine.receive(FRAME, tsf=0, rssi=-40, key_index=None)
        rig.engine.receive(FRAME + b"!", tsf=0, rssi=-40, key_index=None)
        self.assertEqual(rig.memory.word(BASE + 0x480 + 28) & 0xFFFF, len(FRAME) + 1)

    def test_a_descriptor_the_firmware_has_not_recycled_is_left_alone(self):
        rig, _ = self.rig_with_rx()
        rig.engine.receive(FRAME, tsf=0, rssi=-40, key_index=None)
        rig.engine.receive(FRAME, tsf=0, rssi=-40, key_index=None)  # chain has no next RHD
        self.assertEqual(len(rig.raised), 1)


if __name__ == "__main__":
    unittest.main()
