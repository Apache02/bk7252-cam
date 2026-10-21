import os
import select
import time
import unittest
from unittest import mock

from host.endpoint import make_endpoint
from host.pty_endpoint import PtyEndpoint
from host.stdio_endpoint import StdioEndpoint


def read_until(read, want, timeout=2.0):
    out = b""
    deadline = time.time() + timeout
    while len(out) < want and time.time() < deadline:
        out += read()
        time.sleep(0.005)
    return out


def fd_read(fd):
    ready, _, _ = select.select([fd], [], [], 0)
    return os.read(fd, 4096) if ready else b""


class PtyEndpointTest(unittest.TestCase):
    def setUp(self):
        self.endpoint = PtyEndpoint()
        self.addCleanup(self.endpoint.close)

    def open_peer(self):
        fd = os.open(self.endpoint.description(), os.O_RDWR | os.O_NOCTTY)
        self.addCleanup(self.close_quietly, fd)
        return fd

    @staticmethod
    def close_quietly(fd):
        try:
            os.close(fd)
        except OSError:
            pass  # the test already closed it

    def test_roundtrip(self):
        peer = self.open_peer()
        os.write(peer, b"ls\r")
        self.assertEqual(read_until(self.endpoint.read_nowait, 3), b"ls\r")
        self.endpoint.write(b"ok\r\n")
        self.assertEqual(read_until(lambda: fd_read(peer), 4), b"ok\r\n")

    def test_read_nowait_is_empty_when_idle(self):
        self.assertEqual(self.endpoint.read_nowait(), b"")

    def test_survives_peer_close_and_reconnect(self):
        peer = self.open_peer()
        os.close(peer)
        self.endpoint.write(b"lost?")
        self.assertEqual(self.endpoint.read_nowait(), b"")
        again = self.open_peer()
        os.write(again, b"x")
        self.assertEqual(read_until(self.endpoint.read_nowait, 1), b"x")

    def test_writes_never_block_when_nobody_reads(self):
        start = time.time()
        for _ in range(200):
            self.endpoint.write(b"x" * 1024)
        self.assertLess(time.time() - start, 2.0)


class StdioEndpointTest(unittest.TestCase):
    def test_non_tty_roundtrip(self):
        in_r, in_w = os.pipe()
        out_r, out_w = os.pipe()
        endpoint = StdioEndpoint(stdin_fd=in_r, stdout_fd=out_w)
        self.addCleanup(endpoint.close)
        for fd in (in_r, in_w, out_r, out_w):
            self.addCleanup(os.close, fd)
        os.write(in_w, b"abc")
        self.assertEqual(read_until(endpoint.read_nowait, 3), b"abc")
        endpoint.write(b"hi")
        self.assertEqual(read_until(lambda: fd_read(out_r), 2), b"hi")


class MakeEndpointTest(unittest.TestCase):
    def test_pty(self):
        endpoint = make_endpoint("pty")
        self.addCleanup(endpoint.close)
        self.assertIsInstance(endpoint, PtyEndpoint)

    def test_stdio(self):
        with mock.patch("host.endpoint.StdioEndpoint") as stdio:
            self.assertIs(make_endpoint("stdio"), stdio.return_value)

    def test_device_path_selects_serial(self):
        with mock.patch("host.endpoint.SerialEndpoint") as serial:
            endpoint = make_endpoint("/dev/ttyUSB7")
        serial.assert_called_once_with("/dev/ttyUSB7")
        self.assertIs(endpoint, serial.return_value)


if __name__ == "__main__":
    unittest.main()
