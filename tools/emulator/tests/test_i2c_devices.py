import unittest

from i2c_devices import DEVICES, parse_device_spec
from i2c_devices.hi704 import Hi704
from i2c_devices.register_device import RegisterDevice


def write(device, *data):
    assert device.begin(False)
    results = [device.write(byte) for byte in data]
    device.stop()
    return results


def read(device, count):
    assert device.begin(True)
    values = [device.read() for _ in range(count)]
    device.stop()
    return values


class Memory(RegisterDevice):
    NAME = "memory"
    DEFAULT_ADDRESS = 0x50
    READ_ONLY = {0x10: 0xAB}


class RegisterDeviceTest(unittest.TestCase):
    def test_it_starts_zeroed(self):
        self.assertEqual(read(Memory(), 4), [0, 0, 0, 0])

    def test_the_first_byte_of_a_write_sets_the_offset_and_the_next_one_is_stored(self):
        device = Memory()
        write(device, 0x20, 0x55)
        write(device, 0x20)
        self.assertEqual(read(device, 1), [0x55])

    def test_the_offset_moves_on_after_every_byte_read(self):
        device = Memory()
        write(device, 0x20, 1)
        write(device, 0x21, 2)
        write(device, 0x22, 3)
        write(device, 0x20)
        self.assertEqual(read(device, 3), [1, 2, 3])

    def test_the_offset_survives_the_stop_between_the_write_and_the_read(self):
        device = Memory()
        write(device, 0x40, 9)
        write(device, 0x40)
        self.assertEqual(read(device, 1), [9])
        self.assertEqual(read(device, 1), [0])  # continues from 0x41

    def test_several_data_bytes_in_one_write_fill_consecutive_registers(self):
        device = Memory()
        write(device, 0x30, 1, 2, 3)
        write(device, 0x30)
        self.assertEqual(read(device, 3), [1, 2, 3])

    def test_the_offset_wraps_at_256(self):
        device = Memory()
        write(device, 0xFF, 7, 8)
        write(device, 0xFF)
        self.assertEqual(read(device, 2), [7, 8])
        write(device, 0x00)
        self.assertEqual(read(device, 1), [8])

    def test_a_read_only_register_keeps_its_value(self):
        device = Memory()
        write(device, 0x10, 0x00)
        write(device, 0x10)
        self.assertEqual(read(device, 1), [0xAB])

    def test_every_byte_is_acknowledged(self):
        self.assertEqual(write(Memory(), 1, 2, 3), [True, True, True])

    def test_the_offset_of_a_read_only_register_still_advances_when_written(self):
        device = Memory()
        write(device, 0x10, 0x00, 0x77)
        write(device, 0x11)
        self.assertEqual(read(device, 1), [0x77])

    def test_the_default_address_can_be_overridden(self):
        self.assertEqual(Memory().address, 0x50)
        self.assertEqual(Memory(0x51).address, 0x51)


class Hi704Test(unittest.TestCase):
    def test_it_answers_at_0x30_by_default(self):
        self.assertEqual(Hi704().address, 0x30)

    def test_register_4_reads_the_chip_id(self):
        device = Hi704()
        write(device, 0x04)
        self.assertEqual(read(device, 1), [0x96])

    def test_the_chip_id_cannot_be_overwritten(self):
        device = Hi704()
        write(device, 0x04, 0x00)
        write(device, 0x04)
        self.assertEqual(read(device, 1), [0x96])

    def test_init_table_style_writes_read_back(self):
        device = Hi704()
        for register, value in ((0x03, 0x02), (0x13, 0x40), (0x80, 0x2E)):
            write(device, register, value)
        for register, value in ((0x03, 0x02), (0x13, 0x40), (0x80, 0x2E)):
            write(device, register)
            self.assertEqual(read(device, 1), [value])


class RegistryTest(unittest.TestCase):
    def test_hi704_is_known(self):
        self.assertIs(DEVICES["hi704"], Hi704)

    def test_a_type_alone_uses_the_default_address(self):
        device = parse_device_spec("hi704")
        self.assertIsInstance(device, Hi704)
        self.assertEqual(device.address, 0x30)

    def test_an_address_can_follow_an_at_sign(self):
        self.assertEqual(parse_device_spec("hi704@0x31").address, 0x31)
        self.assertEqual(parse_device_spec("hi704@49").address, 49)

    def test_the_type_is_case_insensitive(self):
        self.assertIsInstance(parse_device_spec("HI704"), Hi704)

    def test_an_unknown_type_lists_the_known_ones(self):
        with self.assertRaisesRegex(ValueError, "hi704"):
            parse_device_spec("nope")

    def test_bad_addresses_are_rejected(self):
        for spec in ("hi704@", "hi704@zz", "hi704@0x78", "hi704@7", "hi704@-1"):
            with self.subTest(spec=spec):
                with self.assertRaisesRegex(ValueError, "--i2c1"):
                    parse_device_spec(spec)


if __name__ == "__main__":
    unittest.main()
