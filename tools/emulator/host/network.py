"""The firmware's network: a virtual wifi station, the host IP stack behind it, and the
published ports. This is the "air" object the wifi MAC module is attached to.

The MAC hands it every frame the firmware transmits and calls `tick()` as device time
advances; the station's replies come back through the callback given to `attach()`. The
published host sockets are serviced from `tick()`, at most once per `poll_interval` seconds
of wall time.
"""

import time

from host.net.forward import Forwarder
from host.net.stack import Stack
from host.wifi_station import Station

STATION_MAC = bytes.fromhex("020000000001")
POLL_INTERVAL = 0.002


class Network:
    def __init__(self, publishes, mac=STATION_MAC, ssid=None, poll_interval=POLL_INTERVAL):
        self._station = Station(mac, ssid)
        self._stack = Stack(mac, output=self._station.send_ethernet)
        self._station.stack = self._stack
        self._forwarder = Forwarder(self._stack, publishes)
        self.addresses = self._forwarder.addresses
        self._poll_interval = poll_interval
        self._polled = 0.0

    def attach(self, deliver):
        self._station.attach(deliver)

    def transmit(self, frame):
        self._station.transmit(frame)

    def tick(self, now_ns):
        self._station.tick(now_ns)
        wall = time.monotonic()
        if wall - self._polled >= self._poll_interval:
            self._polled = wall
            self._forwarder.poll(now_ns)

    def next_event_ns(self):
        return self._station.next_event_ns()

    def reset(self):
        """The chip was reset: the link is gone, so are the connections."""
        self._forwarder.reset()
        self._stack.reset()
        self._station.reset()

    def close(self):
        self._forwarder.close()
