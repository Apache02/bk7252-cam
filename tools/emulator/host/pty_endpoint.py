import os
import tty


class PtyEndpoint:
    """A pseudo-terminal; open the slave path with a terminal program like a board.

    The emulator keeps its own slave descriptor open, so a terminal program
    closing and reopening the path never makes the master side fail.
    """

    def __init__(self):
        self._master, self._slave = os.openpty()
        tty.setraw(self._slave)
        os.set_blocking(self._master, False)
        self._path = os.ttyname(self._slave)

    def description(self):
        return self._path

    def write(self, data):
        while data:
            try:
                sent = os.write(self._master, data)
            except OSError:
                return  # buffer full or peer gone: drop, like an unplugged cable
            data = data[sent:]

    def read_nowait(self):
        try:
            return os.read(self._master, 4096)
        except OSError:
            return b""

    def close(self):
        for fd in (self._master, self._slave):
            try:
                os.close(fd)
            except OSError:
                pass
