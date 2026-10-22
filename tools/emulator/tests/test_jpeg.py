import pathlib
import struct
import tempfile
import unittest

from modules.gdma import RequestLine
from modules.jpeg.module import JpegModule

PICTURE = pathlib.Path(__file__).parents[1] / "modules" / "jpeg" / "picture.jpg"

MS = 1_000_000
CTRL0, CTRL1, FIFO, STATUS, BYTES, STATE, QUANT = 0x00, 0x04, 0x14, 0x18, 0x1C, 0x20, 0x80
START_INT, END_INT = 1 << 2, 1 << 3
BYTE_REV, ENC_EN, ENC_SIZE = 1 << 1, 1 << 4, 1 << 16
START, END = 1, 2
INTERVAL, ENCODE = 1000, 150


class Clock:
    def __init__(self):
        self.now = 0

    def now_ns(self):
        return self.now


class Line:
    def __init__(self):
        self.level = False

    def set(self, level):
        self.level = level


def ctrl1(rev=True, size=True, enable=True, x=80, y=60):
    return (BYTE_REV if rev else 0) | (ENC_SIZE if size else 0) | (ENC_EN if enable else 0) | x << 8 | y << 24


def make(**params):
    clock, line, port = Clock(), Line(), RequestLine()
    jpeg = JpegModule(
        "JPEG", clock=clock, irq=line, dma=port,
        frame_interval_ms=INTERVAL, encode_ms=ENCODE, **params,
    )
    return jpeg, clock, line, port


def reg(jpeg, offset, value=None):
    if value is None:
        return jpeg.read("regs", offset, 4)
    jpeg.write("regs", offset, 4, value)


def advance(jpeg, clock, ms):
    clock.now = ms * MS
    jpeg.tick(clock.now)


def empty(jpeg):
    return bool(reg(jpeg, STATE) & 1)


def drain(jpeg):
    data = b""
    while not empty(jpeg):
        data += struct.pack("<I", reg(jpeg, FIFO))
    return data


def running(**kw):
    """A block that has begun its first frame."""
    jpeg, clock, line, port = make()
    reg(jpeg, CTRL1, ctrl1(**kw))
    advance(jpeg, clock, INTERVAL)
    return jpeg, clock, line, port


class FrameTimingTest(unittest.TestCase):
    def test_it_starts_empty_and_idle(self):
        jpeg, _, line, _ = make()
        self.assertTrue(empty(jpeg))
        self.assertEqual((reg(jpeg, STATUS), reg(jpeg, BYTES)), (0, 0))
        self.assertIsNone(jpeg.next_event_ns())
        self.assertFalse(line.level)

    def test_the_first_frame_starts_one_interval_after_the_encoder_is_enabled(self):
        jpeg, clock, _, _ = make()
        advance(jpeg, clock, 300)
        reg(jpeg, CTRL1, ctrl1())
        self.assertEqual(jpeg.next_event_ns(), (300 + INTERVAL) * MS)
        advance(jpeg, clock, 300 + INTERVAL - 1)
        self.assertEqual(reg(jpeg, STATUS), 0)
        advance(jpeg, clock, 300 + INTERVAL)
        self.assertEqual(reg(jpeg, STATUS), START)

    def test_the_start_makes_data_available(self):
        jpeg, _, _, _ = running()
        self.assertFalse(empty(jpeg))

    def test_the_frame_ends_some_milliseconds_after_it_starts(self):
        jpeg, clock, _, _ = running()
        advance(jpeg, clock, INTERVAL + ENCODE - 1)
        self.assertEqual(reg(jpeg, STATUS), START)
        advance(jpeg, clock, INTERVAL + ENCODE)
        self.assertEqual(reg(jpeg, STATUS), START | END)

    def test_frames_repeat_once_per_interval(self):
        jpeg, clock, _, _ = running()
        advance(jpeg, clock, INTERVAL + ENCODE)
        reg(jpeg, STATUS, START | END)
        self.assertEqual(jpeg.next_event_ns(), 2 * INTERVAL * MS)
        advance(jpeg, clock, 2 * INTERVAL)
        self.assertEqual(reg(jpeg, STATUS), START)

    def test_status_bits_clear_when_written_with_one(self):
        jpeg, clock, _, _ = running()
        advance(jpeg, clock, INTERVAL + ENCODE)
        reg(jpeg, STATUS, START)
        self.assertEqual(reg(jpeg, STATUS), END)

    def test_disabling_the_encoder_stops_the_frames_and_empties_the_fifo(self):
        jpeg, clock, _, _ = running()
        reg(jpeg, CTRL1, ctrl1(enable=False))
        self.assertTrue(empty(jpeg))
        self.assertIsNone(jpeg.next_event_ns())
        advance(jpeg, clock, 3 * INTERVAL)
        self.assertEqual(reg(jpeg, STATUS), START)

    def test_a_reset_drops_everything(self):
        jpeg, clock, line, _ = running()
        reg(jpeg, CTRL0, START_INT)
        jpeg.reset()
        self.assertEqual((reg(jpeg, CTRL0), reg(jpeg, CTRL1), reg(jpeg, STATUS)), (0, 0, 0))
        self.assertTrue(empty(jpeg))
        self.assertIsNone(jpeg.next_event_ns())
        self.assertFalse(line.level)

    def test_a_late_tick_catches_up_with_every_event_that_is_due(self):
        jpeg, clock, _, _ = make()
        reg(jpeg, CTRL1, ctrl1())
        advance(jpeg, clock, 5000)
        self.assertEqual(reg(jpeg, STATUS), START | END)
        self.assertEqual(jpeg.next_event_ns(), (5 * INTERVAL + ENCODE) * MS)


class InterruptTest(unittest.TestCase):
    def test_the_line_follows_the_enabled_pending_flags(self):
        jpeg, clock, line, _ = make()
        reg(jpeg, CTRL0, END_INT)
        reg(jpeg, CTRL1, ctrl1())
        advance(jpeg, clock, INTERVAL)
        self.assertFalse(line.level)
        advance(jpeg, clock, INTERVAL + ENCODE)
        self.assertTrue(line.level)
        reg(jpeg, STATUS, END)
        self.assertFalse(line.level)

    def test_the_start_interrupt_has_its_own_enable(self):
        jpeg, clock, line, _ = make()
        reg(jpeg, CTRL0, START_INT)
        reg(jpeg, CTRL1, ctrl1())
        advance(jpeg, clock, INTERVAL)
        self.assertTrue(line.level)

    def test_a_flag_that_was_pending_raises_the_line_once_it_is_enabled(self):
        jpeg, clock, line, _ = running()
        self.assertFalse(line.level)
        reg(jpeg, CTRL0, START_INT)
        self.assertTrue(line.level)


class StreamTest(unittest.TestCase):
    def test_with_byte_reverse_the_words_hold_the_stream_in_natural_order(self):
        jpeg, _, _, _ = running(rev=True)
        self.assertEqual(reg(jpeg, FIFO), 0xE0FFD8FF)

    def test_without_byte_reverse_every_word_is_reversed(self):
        jpeg, _, _, _ = running(rev=False)
        self.assertEqual(reg(jpeg, FIFO), 0xFFD8FFE0)

    def test_the_frame_is_a_jpeg_followed_by_a_zero_byte_and_its_size(self):
        jpeg, clock, _, _ = running()
        advance(jpeg, clock, INTERVAL + ENCODE)
        size = reg(jpeg, BYTES)
        data = drain(jpeg)
        self.assertEqual(data[:2], b"\xFF\xD8")
        self.assertEqual(data[size - 2:size], b"\xFF\xD9")
        self.assertEqual(data[size:], b"\x00" + struct.pack("<I", size))
        self.assertEqual(len(data) % 4, 0)

    def test_without_enc_size_the_frame_has_no_trailer(self):
        jpeg, clock, _, _ = running(size=False)
        advance(jpeg, clock, INTERVAL + ENCODE)
        data = drain(jpeg)
        self.assertEqual(data.rstrip(b"\x00")[-2:], b"\xFF\xD9")
        self.assertEqual(len(data), (reg(jpeg, BYTES) + 3) // 4 * 4)

    def test_reading_an_empty_fifo_gives_the_size_of_the_last_frame_with_enc_size(self):
        jpeg, clock, _, _ = running()
        advance(jpeg, clock, INTERVAL + ENCODE)
        drain(jpeg)
        self.assertEqual(reg(jpeg, FIFO), reg(jpeg, BYTES))
        self.assertTrue(empty(jpeg))

    def test_reading_an_empty_fifo_gives_zero_without_enc_size(self):
        jpeg, clock, _, _ = running(size=False)
        advance(jpeg, clock, INTERVAL + ENCODE)
        drain(jpeg)
        self.assertEqual(reg(jpeg, FIFO), 0)

    def test_the_size_register_reads_zero_before_the_first_frame_ends(self):
        jpeg, _, _, _ = running()
        self.assertEqual(reg(jpeg, BYTES), 0)

    def test_the_size_register_is_read_only(self):
        jpeg, clock, _, _ = running()
        advance(jpeg, clock, INTERVAL + ENCODE)
        size = reg(jpeg, BYTES)
        reg(jpeg, BYTES, 5)
        self.assertEqual(reg(jpeg, BYTES), size)

    def test_data_left_unread_is_dropped_when_the_next_frame_starts(self):
        jpeg, clock, _, _ = running()
        first = reg(jpeg, FIFO)
        reg(jpeg, FIFO)
        advance(jpeg, clock, 2 * INTERVAL)
        self.assertEqual(reg(jpeg, FIFO), first)

    def test_the_frame_is_the_picture_file(self):
        jpeg, clock, _, _ = running()
        advance(jpeg, clock, INTERVAL + ENCODE)
        data = drain(jpeg)[:reg(jpeg, BYTES)]
        self.assertEqual(data, PICTURE.read_bytes())

    def test_another_file_can_be_shown_and_is_padded_to_whole_words_with_its_trailer(self):
        for length in range(20, 25):
            with tempfile.TemporaryDirectory() as folder:
                path = pathlib.Path(folder, "other.jpg")
                path.write_bytes(b"\xFF\xD8" + bytes(length - 4) + b"\xFF\xD9")
                jpeg, clock, _, _ = make(picture=str(path))
                reg(jpeg, CTRL1, ctrl1())
                advance(jpeg, clock, INTERVAL + ENCODE)
                size = reg(jpeg, BYTES)
                data = drain(jpeg)
                self.assertEqual(len(data), size + 5, length)
                self.assertEqual(data[size - 2:size], b"\xFF\xD9", length)
                self.assertIn(path.read_bytes()[2:], data)

    def test_the_quantization_table_ram_reads_back(self):
        jpeg, _, _, _ = make()
        reg(jpeg, QUANT + 0x7C, 0xCAFEF00D)
        self.assertEqual(reg(jpeg, QUANT + 0x7C), 0xCAFEF00D)


class DmaTest(unittest.TestCase):
    def test_the_request_line_is_up_while_the_fifo_holds_words(self):
        jpeg, clock, _, port = make()
        self.assertFalse(port.level)
        reg(jpeg, CTRL1, ctrl1())
        advance(jpeg, clock, INTERVAL)
        self.assertTrue(port.level)
        drain(jpeg)
        self.assertFalse(port.level)

    def test_the_dma_reads_the_same_words_as_the_cpu(self):
        jpeg, _, _, port = running()
        twin, _, _, _ = running()
        words = drain(twin)
        self.assertEqual(port.read(), words[0:4])
        self.assertEqual(struct.pack("<I", reg(jpeg, FIFO)), words[4:8])

    def test_disabling_the_encoder_drops_the_request_line(self):
        jpeg, _, _, port = running()
        reg(jpeg, CTRL1, ctrl1(enable=False))
        self.assertFalse(port.level)


class LoggingTest(unittest.TestCase):
    def test_frame_boundaries_are_logged_for_debugging(self):
        jpeg, clock, _, _ = make()
        reg(jpeg, CTRL1, ctrl1())
        with self.assertLogs("emulator.jpeg", level="DEBUG") as logs:
            advance(jpeg, clock, INTERVAL + ENCODE)
        text = "\n".join(logs.output)
        self.assertIn("start_frame", text)
        self.assertIn("end_frame", text)


if __name__ == "__main__":
    unittest.main()
