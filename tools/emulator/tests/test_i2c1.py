import unittest

from i2c_devices.device import I2cDevice
from modules.i2c1 import I2c1Module

CONFIG, DATA = 0x00, 0x04
ENSMB, STA, STO, ACK_TX, TX_MODE = 1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4
SI, ACK_RX, ACK_REQ, BUSY = 1 << 16, 1 << 17, 1 << 18, 1 << 19
FREQ_DIV_100K = 84 << 6
US = 1000
BYTE_100K_NS = 9 * (6 + 3 * 85) * 1_000_000_000 // 26_000_000


class Clock:
    def __init__(self):
        self.now = 0

    def now_ns(self):
        return self.now


class Line:
    def __init__(self):
        self.level = False

    def set(self, level):
        self.level = level


class Recorder(I2cDevice):
    NAME = "recorder"
    DEFAULT_ADDRESS = 0x30

    def __init__(self, address=None):
        super().__init__(address)
        self.events = []
        self.to_read = [0xA1, 0xA2, 0xA3]
        self.ack_writes = True

    def begin(self, read):
        self.events.append(("begin", read))
        return True

    def write(self, byte):
        self.events.append(("write", byte))
        return self.ack_writes

    def read(self):
        self.events.append(("read",))
        return self.to_read.pop(0)

    def stop(self):
        self.events.append(("stop",))


class Rig:
    def __init__(self, with_device=True):
        self.clock, self.line = Clock(), Line()
        self.i2c = I2c1Module("I2C1", clock=self.clock, irq=self.line, ref_hz=26_000_000)
        self.device = Recorder()
        if with_device:
            self.i2c.attach(self.device)

    def config(self, value=None):
        if value is None:
            return self.i2c.read("regs", CONFIG, 4)
        self.i2c.write("regs", CONFIG, 4, value)

    def data(self, value=None):
        if value is None:
            return self.i2c.read("regs", DATA, 4)
        self.i2c.write("regs", DATA, 4, value)

    def advance(self, ns):
        self.clock.now += ns
        self.i2c.tick(self.clock.now)

    def wait_si(self, limit_ns=10 * BYTE_100K_NS):
        """Run the device clock to the next event; True when `si` came up."""
        end = self.clock.now + limit_ns
        while not self.config() & SI and self.clock.now < end:
            due = self.i2c.next_event_ns()
            if due is None:
                return False
            self.advance(max(due - self.clock.now, 1))
        return bool(self.config() & SI)

    def transfer(self, addr7, payload, is_tx):
        """The sequence of src/platform/drivers/hardware_i2c/i2c1.c, register for register."""
        received = []
        remain = len(payload) if is_tx else payload
        data = list(payload) if is_tx else None
        self.data(addr7 << 1 | (0 if is_tx else 1))
        self.config(self.config() | ENSMB | STA | TX_MODE)
        done = nack = False
        while not done:
            if not self.wait_si():
                self.config((self.config() & ~STA) | STO)
                return "timeout", received
            status = self.config()
            if status & STA:
                status &= ~STA
                self.config(status)
                if not status & ACK_RX:
                    nack = True
                    status |= STO
                    self.config(status)
                    done = True
                elif not is_tx:
                    status &= ~TX_MODE
                    self.config(status)
                elif remain > 0:
                    self.data(data.pop(0))
                    remain -= 1
                else:
                    status |= STO
                    self.config(status)
                    done = True
            elif is_tx:
                if not status & ACK_RX or remain == 0:
                    status |= STO
                    self.config(status)
                    done = True
                else:
                    self.data(data.pop(0))
                    remain -= 1
            else:
                received.append(self.data() & 0xFF)
                remain -= 1
                if remain:
                    status |= ACK_TX
                    self.config(status)
                else:
                    status &= ~ACK_TX
                    self.config(status)
                    status |= STO
                    self.config(status)
                    done = True
            status &= ~SI
            self.config(status)
        self.advance(10 * BYTE_100K_NS)
        return ("nack" if nack else "ok"), received


class RegisterTest(unittest.TestCase):
    def test_the_block_is_two_registers(self):
        self.assertEqual(Rig().i2c.ranges(), [("regs", 8)])

    def test_control_fields_read_back(self):
        rig = Rig()
        rig.config(ENSMB | FREQ_DIV_100K)
        self.assertEqual(rig.config(), ENSMB | FREQ_DIV_100K)

    def test_status_bits_cannot_be_written(self):
        rig = Rig()
        rig.config(ACK_RX | ACK_REQ | BUSY)
        self.assertEqual(rig.config() & (ACK_RX | ACK_REQ | BUSY), 0)

    def test_the_data_register_holds_a_byte(self):
        rig = Rig()
        rig.data(0x1FF)
        self.assertEqual(rig.data(), 0xFF)

    def test_reset_clears_everything(self):
        rig = Rig()
        rig.config(ENSMB | FREQ_DIV_100K)
        rig.i2c.reset()
        self.assertEqual((rig.config(), rig.data()), (0, 0))


class AddressPhaseTest(unittest.TestCase):
    def start(self, rig, addr7=0x30, read=False):
        rig.data(addr7 << 1 | int(read))
        rig.config(ENSMB | STA | TX_MODE | FREQ_DIV_100K)

    def test_start_makes_the_bus_busy_and_si_comes_after_a_byte_time(self):
        rig = Rig()
        self.start(rig)
        self.assertTrue(rig.config() & BUSY)
        self.assertFalse(rig.config() & SI)
        rig.advance(BYTE_100K_NS - 1)
        self.assertFalse(rig.config() & SI)
        rig.advance(1)
        self.assertTrue(rig.config() & SI)
        self.assertTrue(rig.line.level)

    def test_a_present_device_acks_its_address(self):
        rig = Rig()
        self.start(rig)
        rig.advance(BYTE_100K_NS)
        self.assertTrue(rig.config() & ACK_RX)
        self.assertEqual(rig.device.events, [("begin", False)])

    def test_the_read_bit_is_passed_to_the_device(self):
        rig = Rig()
        self.start(rig, read=True)
        rig.advance(BYTE_100K_NS)
        self.assertEqual(rig.device.events, [("begin", True)])

    def test_a_missing_device_nacks(self):
        rig = Rig()
        self.start(rig, addr7=0x31)
        rig.advance(BYTE_100K_NS)
        self.assertTrue(rig.config() & SI)
        self.assertFalse(rig.config() & ACK_RX)

    def test_the_byte_time_follows_the_clock_divider(self):
        rig = Rig()
        rig.data(0x60)
        rig.config(ENSMB | STA | TX_MODE | 1023 << 6)
        slow = 9 * (6 + 3 * 1024) * 1_000_000_000 // 26_000_000
        rig.advance(slow - 1)
        self.assertFalse(rig.config() & SI)
        rig.advance(1)
        self.assertTrue(rig.config() & SI)

    def test_the_next_event_is_when_si_comes_up(self):
        rig = Rig()
        self.assertIsNone(rig.i2c.next_event_ns())
        self.start(rig)
        self.assertEqual(rig.i2c.next_event_ns(), BYTE_100K_NS)

    def test_nothing_starts_while_the_engine_is_disabled(self):
        rig = Rig()
        rig.data(0x60)
        rig.config(STA | TX_MODE)
        rig.advance(10 * BYTE_100K_NS)
        self.assertEqual(rig.config() & (SI | BUSY), 0)


class TransferTest(unittest.TestCase):
    def setUp(self):
        self.rig = Rig()
        self.rig.config(FREQ_DIV_100K)

    def test_a_write_delivers_the_bytes_and_ends_with_a_stop(self):
        result, _ = self.rig.transfer(0x30, [0x04, 0x55], is_tx=True)
        self.assertEqual(result, "ok")
        self.assertEqual(self.rig.device.events, [("begin", False), ("write", 0x04), ("write", 0x55), ("stop",)])
        self.assertFalse(self.rig.config() & BUSY)
        self.assertFalse(self.rig.line.level)

    def test_a_read_returns_the_bytes_and_nacks_the_last(self):
        result, received = self.rig.transfer(0x30, 3, is_tx=False)
        self.assertEqual((result, received), ("ok", [0xA1, 0xA2, 0xA3]))
        self.assertEqual(self.rig.device.events[0], ("begin", True))
        self.assertEqual(self.rig.device.events[-1], ("stop",))

    def test_a_missing_device_is_a_nack_and_a_stop(self):
        result, _ = Rig(with_device=False).transfer(0x30, [1], is_tx=True)
        self.assertEqual(result, "nack")

    def test_a_device_that_nacks_a_byte_ends_the_write(self):
        self.rig.device.ack_writes = False
        result, _ = self.rig.transfer(0x30, [1, 2, 3], is_tx=True)
        self.assertEqual(result, "ok")
        self.assertEqual([e for e in self.rig.device.events if e[0] == "write"], [("write", 1)])

    def test_transfers_follow_each_other(self):
        for _ in range(3):
            self.assertEqual(self.rig.transfer(0x30, [1], is_tx=True)[0], "ok")
        self.assertEqual(self.rig.transfer(0x30, 1, is_tx=False)[0], "ok")

    def test_the_stop_command_does_not_stay_set(self):
        self.rig.transfer(0x30, [1], is_tx=True)
        self.assertFalse(self.rig.config() & STO)

    def test_a_stop_without_a_transaction_is_ignored(self):
        self.rig.config(self.rig.config() | STO)
        self.assertEqual(self.rig.device.events, [])
        self.assertFalse(self.rig.config() & STO)

    def test_a_stop_in_the_middle_aborts_the_byte_on_the_wire(self):
        rig = self.rig
        rig.data(0x60)
        rig.config(rig.config() | ENSMB | STA | TX_MODE)
        rig.advance(BYTE_100K_NS)
        rig.config((rig.config() & ~(STA | SI)) | STO)
        rig.advance(10 * BYTE_100K_NS)
        self.assertEqual(rig.device.events[-1], ("stop",))
        self.assertFalse(rig.config() & (BUSY | SI))

    def test_disabling_the_engine_aborts_everything(self):
        rig = self.rig
        rig.data(0x60)
        rig.config(rig.config() | ENSMB | STA | TX_MODE)
        rig.config(0)
        rig.advance(10 * BYTE_100K_NS)
        self.assertEqual(rig.config(), 0)
        self.assertFalse(rig.line.level)
        self.assertIsNone(rig.i2c.next_event_ns())

    def test_ack_req_is_set_while_a_received_byte_waits_for_its_ack(self):
        rig = self.rig
        rig.data(0x61)
        rig.config(rig.config() | ENSMB | STA | TX_MODE)
        rig.wait_si()
        rig.config(rig.config() & ~(STA | TX_MODE))  # address acked, now receiving
        rig.config(rig.config() & ~SI)
        rig.wait_si()
        self.assertTrue(rig.config() & ACK_REQ)
        rig.config(rig.config() & ~SI)
        self.assertFalse(rig.config() & ACK_REQ)


class AttachTest(unittest.TestCase):
    def test_two_devices_cannot_share_an_address(self):
        rig = Rig()
        with self.assertRaisesRegex(ValueError, "0x30"):
            rig.i2c.attach(Recorder())

    def test_devices_at_different_addresses_coexist(self):
        rig = Rig()
        rig.i2c.attach(Recorder(0x31))
        rig.config(FREQ_DIV_100K)
        self.assertEqual(rig.transfer(0x31, [1], is_tx=True)[0], "ok")


if __name__ == "__main__":
    unittest.main()
