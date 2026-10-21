"""Interfaces between the core and hardware modules."""

from typing import Protocol


class IrqLine(Protocol):
    def set(self, level: bool) -> None: ...


class Clock(Protocol):
    def now_ns(self) -> int: ...


class Fault(Exception):
    """A module asks the core to raise a CPU exception ("dabt" or "pabt")."""

    def __init__(self, kind, addr):
        super().__init__("%s at 0x%08X" % (kind, addr))
        self.kind = kind
        self.addr = addr


class Module:
    """One emulated peripheral block. Collaborators arrive as constructor params."""

    loadable = False
    # Ranges served page by page, read-only, with the CPU's fast RAM path.
    demand_ranges = ()

    def __init__(self, name, **params):
        self.name = name
        self.params = params
        # Replaced by the bus: drop cached pages of a demand range after its data changed.
        self.invalidate = lambda range_name, offset, size: None

    def ranges(self):
        """(range_name, size) pairs; the profile gives each a base address."""
        return []

    def read(self, range_name, offset, size):
        return 0

    def write(self, range_name, offset, size, value):
        pass

    def page(self, range_name, offset):
        """Demand ranges: (4 KiB of data, [(start, end)] page-relative bad spans).

        An access into a bad span goes to read(), which raises the Fault.
        """
        raise NotImplementedError

    def tick(self, now_ns):
        pass

    def next_event_ns(self):
        """Device time of the next self-scheduled event, or None."""
        return None

    def reset(self):
        pass

    def check_load(self, offset, size):
        """Raise ValueError if load(offset, <size bytes>) would be refused."""

    def load(self, offset, data):
        raise NotImplementedError("%s is not loadable" % self.name)
