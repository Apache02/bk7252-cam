"""Register fields other modules can watch (clock gates, power keys and the like).

A module that owns the register declares its signals in its `signals` param:

    gate  = [0x08, 8]                                   # register bit 8 is set
    mac   = { offset = 0x10C, shift = 16, width = 16, equals = 1 }   # field equals a value

A signal is active while its field holds `equals` (1 by default).
"""


class FieldSignal:
    def __init__(self, read_word, offset, shift, width, equals):
        self._read_word = read_word
        self._offset = offset
        self._shift = shift
        self._mask = (1 << width) - 1
        self._equals = equals

    def active(self):
        return (self._read_word(self._offset) >> self._shift) & self._mask == self._equals


def make_signals(read_word, spec):
    """{name: FieldSignal}; `read_word(offset)` reads the owner's register."""
    return {name: _make(read_word, name, value) for name, value in spec.items()}


def _make(read_word, name, value):
    if isinstance(value, (list, tuple)) and len(value) == 2:
        offset, bit = value
        return FieldSignal(read_word, offset, bit, 1, 1)
    if isinstance(value, dict) and "offset" in value:
        return FieldSignal(
            read_word, value["offset"], value.get("shift", 0), value.get("width", 1),
            value.get("equals", 1),
        )
    raise ValueError("signal %s: expected [offset, bit] or {offset, shift, width, equals}" % name)
