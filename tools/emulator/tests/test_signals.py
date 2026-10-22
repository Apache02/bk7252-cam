import unittest

from core.signals import make_signals

GATE_OFF = {"offset": 0x08, "shift": 8}
MAC_UP = {"offset": 0x10C, "shift": 16, "width": 16, "equals": 1}


def signals(words, spec):
    return make_signals(lambda offset: words.get(offset, 0), spec)


class SignalTest(unittest.TestCase):
    def test_bit_list_form(self):
        words = {}
        gate = signals(words, {"gate": [0x08, 8]})["gate"]
        self.assertFalse(gate.active())
        words[0x08] = 1 << 8
        self.assertTrue(gate.active())
        words[0x08] = 0
        self.assertFalse(gate.active())

    def test_field_form_is_active_only_on_the_given_value(self):
        words = {}
        mac = signals(words, {"mac": MAC_UP})["mac"]
        self.assertFalse(mac.active())
        words[0x10C] = 0x0001 << 16
        self.assertTrue(mac.active())
        words[0x10C] = 0xD802 << 16
        self.assertFalse(mac.active())
        words[0x10C] = 0x0001  # the low half is another field
        self.assertFalse(mac.active())

    def test_width_and_equals_default_to_one_bit_set(self):
        words = {0x08: 1 << 8}
        self.assertTrue(signals(words, {"gate": GATE_OFF})["gate"].active())

    def test_signals_read_the_register_each_time(self):
        words = {0x08: 0}
        gate = signals(words, {"gate": GATE_OFF})["gate"]
        words[0x08] = 1 << 8
        self.assertTrue(gate.active())

    def test_unknown_form_rejected_naming_the_signal(self):
        with self.assertRaisesRegex(ValueError, "gate"):
            signals({}, {"gate": "oops"})

    def test_missing_offset_rejected_naming_the_signal(self):
        with self.assertRaisesRegex(ValueError, "gate"):
            signals({}, {"gate": {"shift": 1}})


if __name__ == "__main__":
    unittest.main()
