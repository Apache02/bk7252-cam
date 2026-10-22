"""Port publishing: host ports whose traffic goes to ports of the firmware.

`-p [HOST_IP:]HOST_PORT:GUEST_PORT[/tcp|/udp]`, the way Docker writes it. A TCP publish
listens on the host and opens a connection to the firmware for every client; a UDP publish
keeps one firmware-side port per client address, so replies find their way back. The
host sockets are non-blocking and serviced from `poll()`.
"""

import ipaddress
import logging
import socket
from typing import NamedTuple

log = logging.getLogger("emulator.net.forward")

DEFAULT_HOST_IP = "127.0.0.1"
UDP_IDLE_NS = 60 * 1_000_000_000
WAIT_FOR_ADDRESS_NS = 30 * 1_000_000_000
_RECV_SIZE = 16384
_UDP_SIZE = 65535
_USAGE = "want [HOST_IP:]HOST_PORT:GUEST_PORT[/tcp|/udp]"


class Publish(NamedTuple):
    host_ip: str
    host_port: int
    guest_port: int
    protocol: str


class ForwardError(Exception):
    pass


def parse_publish(text):
    spec, _, protocol = text.partition("/")
    protocol = protocol if "/" in text else "tcp"
    parts = spec.split(":")
    try:
        if protocol not in ("tcp", "udp") or len(parts) not in (2, 3):
            raise ValueError
        host_ip = str(ipaddress.IPv4Address(parts[0])) if len(parts) == 3 else DEFAULT_HOST_IP
        host_port, guest_port = (int(port) for port in parts[-2:])
        if not (1 <= host_port <= 65535 and 1 <= guest_port <= 65535):
            raise ValueError
    except ValueError:
        raise ValueError("bad --publish %r (%s, ports 1-65535)" % (text, _USAGE)) from None
    return Publish(host_ip, host_port, guest_port, protocol)


class _Connection:
    def __init__(self, sock, peer, guest_port, accepted):
        self.sock, self.peer = sock, peer
        self.guest_port, self.accepted = guest_port, accepted
        self.flow = None
        self.to_host = bytearray()
        self.guest_closed = False
        self.reset = False
        self.host_eof = False
        self.shut = False


class _UdpPublish:
    def __init__(self, sock, guest_port):
        self.sock, self.guest_port = sock, guest_port
        self.clients = {}  # client address -> [local port, last use ns]


class Forwarder:
    """`addresses` lists the (host_ip, host_port) each publish really listens on."""

    def __init__(self, stack, publishes):
        self._stack = stack
        self._listeners = []  # (socket, guest_port)
        self._udp = []
        self._connections = []
        self.addresses = []
        try:
            for publish in publishes:
                self._open(publish)
        except OSError as err:
            self.close()
            raise ForwardError(
                "cannot publish %s:%d to guest port %d/%s: %s"
                % (publish.host_ip, publish.host_port, publish.guest_port, publish.protocol, err)
            ) from err

    def _open(self, publish):
        udp = publish.protocol == "udp"
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM if udp else socket.SOCK_STREAM)
        try:
            if not udp:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind((publish.host_ip, publish.host_port))
            if not udp:
                sock.listen()
            sock.setblocking(False)
        except OSError:
            sock.close()
            raise
        if udp:
            self._udp.append(_UdpPublish(sock, publish.guest_port))
        else:
            self._listeners.append((sock, publish.guest_port))
        self.addresses.append(sock.getsockname()[:2])
        log.debug("publishing %s:%d to guest port %d/%s", publish.host_ip, sock.getsockname()[1], publish.guest_port, publish.protocol)

    def reset(self):
        """Drop every connection and UDP client; the listening sockets stay open."""
        for connection in self._connections:
            if connection.flow is not None:
                connection.flow.abort()
            connection.sock.close()
        self._connections = []
        for publish in self._udp:
            for local, _ in publish.clients.values():
                self._stack.unbind_udp(local)
            publish.clients.clear()

    def close(self):
        self.reset()
        for sock, _ in self._listeners:
            sock.close()
        self._listeners = []
        for publish in self._udp:
            publish.sock.close()
        self._udp = []

    # ---- servicing

    def poll(self, now):
        for sock, guest_port in self._listeners:
            self._accept(sock, guest_port, now)
        for connection in list(self._connections):
            self._service(connection, now)
        for publish in self._udp:
            self._service_udp(publish, now)

    def _accept(self, listener, guest_port, now):
        while True:
            try:
                client, peer = listener.accept()
            except (BlockingIOError, InterruptedError):
                return
            except OSError:
                return
            client.setblocking(False)
            self._connections.append(_Connection(client, peer, guest_port, now))

    def _open_flow(self, connection, now):
        connection.flow = self._stack.open_tcp(
            connection.guest_port,
            deliver=connection.to_host.extend,
            on_peer_closed=lambda: setattr(connection, "guest_closed", True),
            on_closed=lambda reset: self._guest_ended(connection, reset),
            now=now,
        )
        if connection.flow is not None:
            log.debug("connection from %s:%d to guest port %d", *connection.peer, connection.guest_port)

    def _guest_ended(self, connection, reset):
        connection.guest_closed = True
        connection.reset = reset

    def _service(self, connection, now):
        if connection.flow is None:
            self._open_flow(connection, now)
        if connection.flow is None:  # the firmware has no address yet: the client waits
            if now - connection.accepted > WAIT_FOR_ADDRESS_NS:
                log.warning("connection from %s:%d dropped: the firmware got no address", *connection.peer)
                self._drop(connection, abort=False)
            return
        flow = connection.flow
        if not connection.host_eof and flow.can_write() and not flow.finished:
            try:
                data = connection.sock.recv(_RECV_SIZE)
            except (BlockingIOError, InterruptedError):
                data = None
            except OSError:
                return self._drop(connection, abort=True)
            if data:
                flow.write(data, now)
            elif data == b"":
                connection.host_eof = True
                flow.close(now)
        if connection.to_host:
            try:
                sent = connection.sock.send(connection.to_host)
                del connection.to_host[:sent]
            except (BlockingIOError, InterruptedError):
                pass
            except OSError:
                return self._drop(connection, abort=True)
        if connection.guest_closed and not connection.to_host and not connection.shut:
            connection.shut = True
            try:
                connection.sock.shutdown(socket.SHUT_WR)
            except OSError:
                pass
        if connection.reset or (flow.finished and not connection.to_host):
            self._drop(connection, abort=False)

    def _drop(self, connection, abort):
        if abort and connection.flow is not None and not connection.flow.finished:
            connection.flow.abort()
        connection.sock.close()
        if connection in self._connections:
            self._connections.remove(connection)
        log.debug("connection from %s:%d closed", *connection.peer)

    def _service_udp(self, publish, now):
        while True:
            try:
                data, client = publish.sock.recvfrom(_UDP_SIZE)
            except (BlockingIOError, InterruptedError):
                break
            except OSError:
                break
            entry = publish.clients.get(client)
            if entry is None:
                local = self._stack.allocate_port()
                entry = publish.clients[client] = [local, now]
                self._stack.bind_udp(local, lambda guest_port, payload, p=publish, c=client: self._udp_reply(p, c, payload))
            entry[1] = now
            if not self._stack.send_udp(entry[0], publish.guest_port, data):
                log.debug("datagram from %s:%d dropped: the firmware has no address yet", *client)
        for client, (local, last) in list(publish.clients.items()):
            if now - last > UDP_IDLE_NS:
                self._stack.unbind_udp(local)
                del publish.clients[client]

    def _udp_reply(self, publish, client, payload):
        try:
            publish.sock.sendto(payload, client)
        except OSError:
            pass
