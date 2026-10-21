class SerialEndpoint:
    """A real host serial device. Needs pyserial."""

    def __init__(self, path):
        try:
            import serial
        except ImportError as err:
            raise RuntimeError(
                "a serial device needs pyserial: pip install pyserial"
            ) from err
        self._path = path
        self._port = serial.Serial(path, baudrate=115200, timeout=0, write_timeout=0)

    def description(self):
        return self._path

    def write(self, data):
        try:
            self._port.write(data)
        except Exception:
            pass  # a stalled or unplugged device must not stop the emulation

    def read_nowait(self):
        try:
            return self._port.read(4096)
        except Exception:
            return b""

    def close(self):
        self._port.close()
