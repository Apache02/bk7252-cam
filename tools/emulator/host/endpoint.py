"""Host side of a UART: where guest bytes go and where typed bytes come from."""

from typing import Protocol

from host.pty_endpoint import PtyEndpoint
from host.serial_endpoint import SerialEndpoint
from host.stdio_endpoint import StdioEndpoint


class ByteEndpoint(Protocol):
    def write(self, data: bytes) -> None: ...

    def read_nowait(self) -> bytes:
        """Bytes that arrived since the last call; empty when none."""
        ...

    def description(self) -> str: ...

    def close(self) -> None: ...


def make_endpoint(spec):
    if spec == "pty":
        return PtyEndpoint()
    if spec == "stdio":
        return StdioEndpoint()
    return SerialEndpoint(spec)
