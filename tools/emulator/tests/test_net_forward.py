import socket
import time
import unittest

from host.net.forward import ForwardError, Forwarder, Publish, parse_publish


class ParseTest(unittest.TestCase):
    def test_host_port_and_guest_port(self):
        self.assertEqual(parse_publish("8080:80"), Publish("127.0.0.1", 8080, 80, "tcp"))

    def test_udp_is_chosen_with_a_suffix(self):
        self.assertEqual(parse_publish("5353:53/udp"), Publish("127.0.0.1", 5353, 53, "udp"))
        self.assertEqual(parse_publish("5353:53/tcp").protocol, "tcp")

    def test_a_host_address_can_come_first(self):
        self.assertEqual(parse_publish("0.0.0.0:8080:80"), Publish("0.0.0.0", 8080, 80, "tcp"))
        self.assertEqual(parse_publish("192.168.1.5:2323:23/udp"), Publish("192.168.1.5", 2323, 23, "udp"))

    def test_bad_specs_are_rejected_with_a_message(self):
        for spec in ("", "80", "a:b", "8080:0", "70000:80", "8080:80/sctp", "1:2:3:4", "999.1.1.1:80:80", "8080:80/"):
            with self.subTest(spec=spec):
                with self.assertRaisesRegex(ValueError, "--publish"):
                    parse_publish(spec)


class FakeFlow:
    def __init__(self, guest_port, deliver, on_peer_closed, on_closed):
        self.guest_port, self.deliver = guest_port, deliver
        self.on_peer_closed, self.on_closed = on_peer_closed, on_closed
        self.written, self.closed, self.aborted = [], False, False
        self.writable = True
        self.finished = False

    def can_write(self):
        return self.writable

    def write(self, data, now):
        self.written.append(data)

    def close(self, now):
        self.closed = True

    def abort(self):
        self.aborted = True
        self.finished = True


class FakeStack:
    def __init__(self):
        self.bound = True
        self.flows = []
        self.udp_sent = []
        self.handlers = {}
        self.ports = iter(range(41000, 42000))

    def open_tcp(self, guest_port, deliver, on_peer_closed, on_closed, now):
        if not self.bound:
            return None
        flow = FakeFlow(guest_port, deliver, on_peer_closed, on_closed)
        self.flows.append(flow)
        return flow

    def allocate_port(self):
        return next(self.ports)

    def bind_udp(self, port, handler):
        self.handlers[port] = handler

    def unbind_udp(self, port):
        self.handlers.pop(port, None)

    def send_udp(self, local_port, guest_port, data):
        self.udp_sent.append((local_port, guest_port, data))
        return True


def wait_for(condition, poll, timeout=2.0):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        poll()
        if condition():
            return True
        time.sleep(0.002)
    return False


class TcpForwardTest(unittest.TestCase):
    def setUp(self):
        self.stack = FakeStack()
        self.forwarder = Forwarder(self.stack, [Publish("127.0.0.1", 0, 80, "tcp")])
        self.addCleanup(self.forwarder.close)
        self.port = self.forwarder.addresses[0][1]
        self.now = 0

    def poll(self):
        self.now += 1000
        self.forwarder.poll(self.now)

    def connect(self):
        client = socket.create_connection(("127.0.0.1", self.port))
        self.addCleanup(client.close)
        client.settimeout(2.0)
        self.assertTrue(wait_for(lambda: self.stack.flows, self.poll))
        return client, self.stack.flows[-1]

    def test_a_connection_opens_a_flow_to_the_guest_port(self):
        _, flow = self.connect()
        self.assertEqual(flow.guest_port, 80)

    def test_bytes_from_the_client_are_written_to_the_flow(self):
        client, flow = self.connect()
        client.sendall(b"GET / HTTP/1.0\r\n\r\n")
        self.assertTrue(wait_for(lambda: flow.written, self.poll))
        self.assertEqual(b"".join(flow.written), b"GET / HTTP/1.0\r\n\r\n")

    def test_bytes_from_the_guest_reach_the_client(self):
        client, flow = self.connect()
        flow.deliver(b"HTTP/1.0 200 OK")
        wait_for(lambda: False, self.poll, timeout=0.05)
        self.assertEqual(client.recv(100), b"HTTP/1.0 200 OK")

    def test_the_guest_closing_ends_the_client_stream(self):
        client, flow = self.connect()
        flow.deliver(b"bye")
        flow.on_peer_closed()
        wait_for(lambda: False, self.poll, timeout=0.05)
        self.assertEqual(client.recv(100), b"bye")
        self.assertEqual(client.recv(100), b"")

    def test_the_client_closing_closes_the_flow(self):
        client, flow = self.connect()
        client.shutdown(socket.SHUT_WR)
        self.assertTrue(wait_for(lambda: flow.closed, self.poll))

    def test_a_full_flow_is_not_fed_more(self):
        client, flow = self.connect()
        flow.writable = False
        client.sendall(b"x" * 100)
        wait_for(lambda: False, self.poll, timeout=0.05)
        self.assertEqual(flow.written, [])
        flow.writable = True
        self.assertTrue(wait_for(lambda: flow.written, self.poll))

    def test_a_reset_by_the_guest_closes_the_client_socket(self):
        client, flow = self.connect()
        flow.finished = True
        flow.on_closed(True)
        wait_for(lambda: False, self.poll, timeout=0.05)
        try:
            data = client.recv(10)
        except ConnectionResetError:
            data = b""
        self.assertEqual(data, b"")

    def test_a_client_waits_until_the_firmware_has_an_address(self):
        self.stack.bound = False
        client = socket.create_connection(("127.0.0.1", self.port))
        self.addCleanup(client.close)
        wait_for(lambda: False, self.poll, timeout=0.05)
        self.assertEqual(self.stack.flows, [])
        self.stack.bound = True
        self.assertTrue(wait_for(lambda: self.stack.flows, self.poll))
        client.sendall(b"hi")
        self.assertTrue(wait_for(lambda: self.stack.flows[0].written, self.poll))

    def test_a_client_that_waits_too_long_is_dropped(self):
        self.stack.bound = False
        client = socket.create_connection(("127.0.0.1", self.port))
        self.addCleanup(client.close)
        client.settimeout(2.0)
        wait_for(lambda: False, self.poll, timeout=0.05)
        with self.assertLogs("emulator.net.forward", level="WARNING"):
            self.forwarder.poll(self.now + 31 * 1_000_000_000)
        self.assertEqual(client.recv(10), b"")

    def test_close_aborts_open_flows(self):
        _, flow = self.connect()
        self.forwarder.close()
        self.assertTrue(flow.aborted)


class UdpForwardTest(unittest.TestCase):
    def setUp(self):
        self.stack = FakeStack()
        self.forwarder = Forwarder(self.stack, [Publish("127.0.0.1", 0, 5000, "udp")])
        self.addCleanup(self.forwarder.close)
        self.port = self.forwarder.addresses[0][1]
        self.now = 0

    def poll(self):
        self.now += 1000
        self.forwarder.poll(self.now)

    def client(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(2.0)
        self.addCleanup(sock.close)
        return sock

    def test_a_datagram_goes_to_the_guest_port(self):
        sock = self.client()
        sock.sendto(b"ping", ("127.0.0.1", self.port))
        self.assertTrue(wait_for(lambda: self.stack.udp_sent, self.poll))
        local, guest, data = self.stack.udp_sent[0]
        self.assertEqual((guest, data), (5000, b"ping"))

    def test_the_reply_goes_back_to_the_sender_from_the_published_port(self):
        sock = self.client()
        sock.sendto(b"ping", ("127.0.0.1", self.port))
        wait_for(lambda: self.stack.udp_sent, self.poll)
        local = self.stack.udp_sent[0][0]
        self.stack.handlers[local](5000, b"pong")
        data, source = sock.recvfrom(100)
        self.assertEqual((data, source[1]), (b"pong", self.port))

    def test_one_client_keeps_one_guest_side_port(self):
        sock = self.client()
        for _ in range(2):
            sock.sendto(b"x", ("127.0.0.1", self.port))
        wait_for(lambda: len(self.stack.udp_sent) == 2, self.poll)
        self.assertEqual(len({sent[0] for sent in self.stack.udp_sent}), 1)

    def test_two_clients_get_different_guest_side_ports(self):
        for _ in range(2):
            self.client().sendto(b"x", ("127.0.0.1", self.port))
        wait_for(lambda: len(self.stack.udp_sent) == 2, self.poll)
        self.assertEqual(len({sent[0] for sent in self.stack.udp_sent}), 2)

    def test_an_idle_client_is_forgotten(self):
        sock = self.client()
        sock.sendto(b"x", ("127.0.0.1", self.port))
        wait_for(lambda: self.stack.udp_sent, self.poll)
        local = self.stack.udp_sent[0][0]
        self.forwarder.poll(self.now + 10 * 60 * 1_000_000_000)
        self.assertNotIn(local, self.stack.handlers)

    def test_a_datagram_without_an_address_is_dropped(self):
        self.stack.send_udp = lambda *args: False
        self.client().sendto(b"x", ("127.0.0.1", self.port))
        wait_for(lambda: False, self.poll, timeout=0.05)


class StartTest(unittest.TestCase):
    def test_a_port_in_use_is_reported(self):
        holder = socket.socket()
        holder.bind(("127.0.0.1", 0))
        holder.listen()
        self.addCleanup(holder.close)
        port = holder.getsockname()[1]
        with self.assertRaisesRegex(ForwardError, str(port)):
            Forwarder(FakeStack(), [Publish("127.0.0.1", port, 80, "tcp")])

    def test_sockets_opened_before_a_failure_are_released(self):
        holder = socket.socket()
        holder.bind(("127.0.0.1", 0))
        holder.listen()
        self.addCleanup(holder.close)
        with self.assertRaises(ForwardError):
            Forwarder(FakeStack(), [Publish("127.0.0.1", 0, 80, "tcp"), Publish("127.0.0.1", holder.getsockname()[1], 80, "tcp")])


if __name__ == "__main__":
    unittest.main()
