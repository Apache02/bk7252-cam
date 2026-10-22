import contextlib
import io
import logging
import os
import signal
import tempfile
import threading
import unittest
from pathlib import Path

import cli
from host.net.forward import ForwardError


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


class FakeNetwork:
    def __init__(self, publishes):
        self.publishes = publishes
        self.addresses = [(p.host_ip, p.host_port) for p in publishes]
        self.deliver = None
        self.closed = False

    def attach(self, deliver):
        self.deliver = deliver

    def transmit(self, frame):
        pass

    def tick(self, now_ns):
        pass

    def next_event_ns(self):
        return None

    def reset(self):
        pass

    def close(self):
        self.closed = True


def run_cli(*argv, networks=None, make_network=None):
    out, err = io.StringIO(), io.StringIO()
    made = []

    def factory(spec):
        endpoint = FakeEndpoint(spec)
        made.append(endpoint)
        return endpoint

    def network_factory(publishes):
        network = FakeNetwork(publishes)
        if networks is not None:
            networks.append(network)
        return network

    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = cli.main(list(argv), make_endpoint=factory, make_network=make_network or network_factory)
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

    def test_debug_option_turns_on_a_modules_debug_log(self):
        logger = logging.getLogger("emulator.gdma")
        self.addCleanup(logger.setLevel, logging.NOTSET)
        timer = threading.Timer(0.2, os.kill, (os.getpid(), signal.SIGINT))
        timer.start()
        self.addCleanup(timer.cancel)
        run_cli("BK7252", "--load", "iram:" + tmp_image(), "--debug", "gdma")
        self.assertEqual(logger.level, logging.DEBUG)
        self.assertEqual(logging.getLogger("emulator.uart").level, logging.NOTSET)

    def test_short_debug_option_takes_a_comma_separated_list(self):
        names = ("gdma", "wifi", "flash")
        for name in names:
            self.addCleanup(logging.getLogger("emulator." + name).setLevel, logging.NOTSET)
        timer = threading.Timer(0.2, os.kill, (os.getpid(), signal.SIGINT))
        timer.start()
        self.addCleanup(timer.cancel)
        run_cli("BK7252", "--load", "iram:" + tmp_image(), "-d", "gdma,wifi", "-d", "flash")
        for name in names:
            self.assertEqual(logging.getLogger("emulator." + name).level, logging.DEBUG, name)

    def test_debug_all_turns_on_every_modules_debug_log(self):
        root = logging.getLogger("emulator")
        self.addCleanup(root.setLevel, logging.INFO)
        self.interrupt_soon()
        run_cli("BK7252", "--load", "iram:" + tmp_image(), "-d", "all")
        for name in ("gdma", "wifi.mac_core", "net.forward"):
            self.assertTrue(logging.getLogger("emulator." + name).isEnabledFor(logging.DEBUG), name)

    def test_without_debug_all_debug_stays_off(self):
        self.interrupt_soon()
        run_cli("BK7252", "--load", "iram:" + tmp_image())
        self.assertFalse(logging.getLogger("emulator.gdma").isEnabledFor(logging.DEBUG))

    def interrupt_soon(self):
        timer = threading.Timer(0.2, os.kill, (os.getpid(), signal.SIGINT))
        timer.start()
        self.addCleanup(timer.cancel)

    def test_publish_builds_the_network_and_attaches_it_to_the_mac(self):
        self.interrupt_soon()
        networks = []
        code, out, _, _ = run_cli(
            "BK7252", "--load", "iram:" + tmp_image(), "-p", "8080:80", "--publish", "5000:5000/udp", networks=networks
        )
        self.assertEqual(code, 0)
        (network,) = networks
        self.assertEqual([(p.host_port, p.guest_port, p.protocol) for p in network.publishes], [(8080, 80, "tcp"), (5000, 5000, "udp")])
        self.assertIsNotNone(network.deliver)  # the MAC took it as its air
        self.assertTrue(network.closed)
        self.assertIn("tcp 127.0.0.1:8080 -> guest port 80", out)
        self.assertIn("udp 127.0.0.1:5000 -> guest port 5000", out)

    def test_without_publish_there_is_no_network(self):
        self.interrupt_soon()
        networks = []
        run_cli("BK7252", "--load", "iram:" + tmp_image(), networks=networks)
        self.assertEqual(networks, [])

    def test_a_bad_publish_is_an_error(self):
        code, _, err, _ = run_cli("BK7252", "--load", "iram:" + tmp_image(), "-p", "80")
        self.assertEqual(code, 2)
        self.assertIn("--publish", err)

    def test_a_port_that_cannot_be_opened_is_an_error(self):
        def failing(publishes):
            raise ForwardError("cannot publish 127.0.0.1:8080: in use")

        code, _, err, made = run_cli("BK7252", "--load", "iram:" + tmp_image(), "-p", "8080:80", make_network=failing)
        self.assertEqual(code, 2)
        self.assertIn("in use", err)
        self.assertEqual(made, [])  # no UART endpoint was created before the failure

    def test_uart_options_default_to_pty(self):
        timer = threading.Timer(0.2, os.kill, (os.getpid(), signal.SIGINT))
        timer.start()
        self.addCleanup(timer.cancel)
        _, _, _, made = run_cli("BK7252", "--load", "iram:" + tmp_image())
        self.assertEqual([e.label for e in made], ["pty", "pty"])


if __name__ == "__main__":
    unittest.main()
