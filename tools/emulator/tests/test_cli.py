import contextlib
import io
import os
import signal
import tempfile
import threading
import unittest
from pathlib import Path

import cli


class FakeEndpoint:
    def __init__(self, label):
        self.label = label
        self.closed = False

    def description(self):
        return "fake:" + self.label

    def write(self, data):
        pass

    def read_nowait(self):
        return b""

    def close(self):
        self.closed = True


def run_cli(*argv):
    out, err = io.StringIO(), io.StringIO()
    made = []

    def factory(spec):
        endpoint = FakeEndpoint(spec)
        made.append(endpoint)
        return endpoint

    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = cli.main(list(argv), make_endpoint=factory)
    return code, out.getvalue(), err.getvalue(), made


def tmp_image(data=b"\x00" * 34):
    path = Path(tempfile.mkdtemp()) / "image.bin"
    path.write_bytes(data)
    return str(path)


class CliTest(unittest.TestCase):
    def test_no_load_is_an_error(self):
        code, _, err, _ = run_cli("BK7252")
        self.assertNotEqual(code, 0)
        self.assertIn("--load", err)

    def test_unknown_profile_lists_available(self):
        code, _, err, _ = run_cli("NOPE", "--load", "iram:" + tmp_image())
        self.assertNotEqual(code, 0)
        self.assertIn("BK7252", err)

    def test_lower_case_profile_rejected(self):
        code, _, err, _ = run_cli("bk7252", "--load", "iram:" + tmp_image())
        self.assertNotEqual(code, 0)
        self.assertIn("upper case", err)

    def test_bad_load_reports_before_start(self):
        code, out, err, made = run_cli("BK7252", "--load", "0x20000000:" + tmp_image())
        self.assertNotEqual(code, 0)
        self.assertIn("0x20000000", err)
        self.assertEqual(made, [])
        self.assertEqual(out, "")

    def test_flash_load_with_bad_size_reports_error(self):
        code, _, err, _ = run_cli("BK7252", "--load", "app:" + tmp_image(b"\x00" * 100))
        self.assertNotEqual(code, 0)
        self.assertIn("100", err)

    def test_sigint_stops_run_loop_cleanly_and_closes_endpoints(self):
        timer = threading.Timer(0.3, os.kill, (os.getpid(), signal.SIGINT))
        timer.start()
        self.addCleanup(timer.cancel)
        code, out, _, made = run_cli("BK7252", "--load", "iram:" + tmp_image(), "--uart1", "a", "--uart2", "b")
        self.assertEqual(code, 0)
        self.assertIn("UART1: fake:a", out)
        self.assertIn("UART2: fake:b", out)
        self.assertTrue(all(e.closed for e in made))

    def test_uart_options_default_to_pty(self):
        timer = threading.Timer(0.2, os.kill, (os.getpid(), signal.SIGINT))
        timer.start()
        self.addCleanup(timer.cancel)
        _, _, _, made = run_cli("BK7252", "--load", "iram:" + tmp_image())
        self.assertEqual([e.label for e in made], ["pty", "pty"])


if __name__ == "__main__":
    unittest.main()
