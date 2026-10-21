import os
import select
import termios
import tty


class StdioEndpoint:
    """The terminal the emulator runs in. A real tty is put in raw mode until close()."""

    def __init__(self, stdin_fd=0, stdout_fd=1):
        self._in = stdin_fd
        self._out = stdout_fd
        self._saved = None
        if os.isatty(self._in):
            self._saved = termios.tcgetattr(self._in)
            tty.setraw(self._in)

    def description(self):
        return "stdio"

    def write(self, data):
        while data:
            try:
                data = data[os.write(self._out, data):]
            except BlockingIOError:
                return
            except OSError:
                return

    def read_nowait(self):
        ready, _, _ = select.select([self._in], [], [], 0)
        if not ready:
            return b""
        try:
            return os.read(self._in, 4096)
        except OSError:
            return b""

    def close(self):
        if self._saved is not None:
            termios.tcsetattr(self._in, termios.TCSADRAIN, self._saved)
            self._saved = None
