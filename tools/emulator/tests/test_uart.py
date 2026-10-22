import unittest

from modules.uart import UartModule

CONFIG, FIFO_CONFIG, FIFO_STATUS, FIFO_DATA, IRQ_ENABLE, IRQ_STATUS = (i * 4 for i in range(6))
TX_NEED_WRITE, RX_NEED_READ, TX_STOP_END = 1 << 0, 1 << 1, 1 << 5
TX_EMPTY, RD_READY, WR_READY, RX_EMPTY = 1 << 17, 1 << 21, 1 << 20, 1 << 19


class FakeEndpoint:
    def __init__(self):
        self.sent = b""
        self.inbox = b""

    def write(self, data):
        self.sent += data

    def read_nowait(self):
        data, self.inbox = self.inbox, b""
        return data


class Line:
    def __init__(self):
        self.level = False

    def set(self, level):
        self.level = level


def make():
    endpoint, line = FakeEndpoint(), Line()
    uart = UartModule("UART2", endpoint=endpoint, irq=line)
    return uart, endpoint, line


def reg(uart, offset, value=None):
    if value is None:
        return uart.read("regs", offset, 4)
    uart.write("regs", offset, 4, value)


class UartTest(unittest.TestCase):
    def test_tx_reaches_endpoint(self):
        uart, endpoint, _ = make()
        for byte in b"hi":
            reg(uart, FIFO_DATA, byte)
        self.assertEqual(endpoint.sent, b"hi")

    def test_tx_fifo_reads_empty_and_writable(self):
        uart, _, _ = make()
        reg(uart, FIFO_DATA, ord("a"))
        status = reg(uart, FIFO_STATUS)
        self.assertTrue(status & TX_EMPTY)
        self.assertTrue(status & WR_READY)
        self.assertEqual(status & 0xFF, 0)

    def test_rx_byte_appears_after_tick_and_pops_from_data_register(self):
        uart, endpoint, _ = make()
        self.assertTrue(reg(uart, FIFO_STATUS) & RX_EMPTY)
        endpoint.inbox = b"A"
        uart.tick(0)
        status = reg(uart, FIFO_STATUS)
        self.assertTrue(status & RD_READY)
        self.assertEqual((status >> 8) & 0xFF, 1)
        self.assertEqual((reg(uart, FIFO_DATA) >> 8) & 0xFF, ord("A"))
        self.assertTrue(reg(uart, FIFO_STATUS) & RX_EMPTY)

    def test_rx_irq_follows_threshold_and_enable(self):
        uart, endpoint, line = make()
        reg(uart, FIFO_CONFIG, 2 << 8)  # rx threshold 2
        reg(uart, IRQ_ENABLE, RX_NEED_READ)
        endpoint.inbox = b"A"
        uart.tick(0)
        self.assertFalse(line.level)
        endpoint.inbox = b"B"
        uart.tick(0)
        self.assertTrue(line.level)
        self.assertTrue(reg(uart, IRQ_STATUS) & RX_NEED_READ)
        reg(uart, FIFO_DATA)
        reg(uart, FIFO_DATA)
        self.assertFalse(line.level)

    def test_rx_irq_masked_when_not_enabled(self):
        uart, endpoint, line = make()
        reg(uart, FIFO_CONFIG, 1 << 8)
        endpoint.inbox = b"A"
        uart.tick(0)
        self.assertFalse(line.level)
        self.assertEqual(reg(uart, IRQ_STATUS), 0)

    def test_tx_need_write_is_level_while_enabled(self):
        uart, _, line = make()
        reg(uart, IRQ_ENABLE, TX_NEED_WRITE)
        self.assertTrue(line.level)
        reg(uart, IRQ_STATUS, TX_NEED_WRITE)
        self.assertTrue(line.level)
        reg(uart, IRQ_ENABLE, 0)
        self.assertFalse(line.level)

    def test_tx_stop_end_latches_after_write_until_cleared(self):
        uart, _, line = make()
        reg(uart, IRQ_ENABLE, TX_STOP_END)
        self.assertFalse(line.level)
        reg(uart, FIFO_DATA, ord("x"))
        self.assertTrue(line.level)
        reg(uart, IRQ_STATUS, TX_STOP_END)
        self.assertFalse(line.level)

    def test_rx_fifo_keeps_overflow_bytes_beyond_depth(self):
        uart, endpoint, _ = make()
        endpoint.inbox = bytes(range(200))
        uart.tick(0)
        self.assertEqual((reg(uart, FIFO_STATUS) >> 8) & 0xFF, 128)
        popped = [(reg(uart, FIFO_DATA) >> 8) & 0xFF for _ in range(128)]
        uart.tick(0)
        popped += [(reg(uart, FIFO_DATA) >> 8) & 0xFF for _ in range(72)]
        self.assertEqual(popped, list(range(200)))

    def test_reset_clears_state_and_keeps_the_endpoint(self):
        uart, endpoint, line = make()
        reg(uart, IRQ_ENABLE, RX_NEED_READ)
        reg(uart, FIFO_CONFIG, 1 << 8)
        endpoint.inbox = b"A"
        uart.tick(0)
        self.assertTrue(line.level)
        uart.reset()
        self.assertFalse(line.level)
        self.assertTrue(reg(uart, FIFO_STATUS) & RX_EMPTY)
        self.assertEqual(reg(uart, IRQ_ENABLE), 0)
        reg(uart, FIFO_DATA, ord("z"))
        self.assertEqual(endpoint.sent, b"z")

    def test_other_registers_read_back(self):
        uart, _, _ = make()
        reg(uart, CONFIG, 0x1234)
        self.assertEqual(reg(uart, CONFIG), 0x1234)


if __name__ == "__main__":
    unittest.main()
