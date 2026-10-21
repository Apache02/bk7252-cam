import unittest

from modules.intc import IntcModule

IRQ_ENABLE = 0x40
GLOBAL_EN = 0x44
RAW = 0x48
STATUS = 0x4C
UART2 = 1  # source number: bit 1 of irq_enable
GLOBAL_IRQ, GLOBAL_FIQ = 1, 2
FIQ_SOURCE = 17


class Line:
    def __init__(self):
        self.level = False

    def set(self, level):
        self.level = level


def make():
    irq, fiq = Line(), Line()
    intc = IntcModule("ICU", cpu_irq=irq, cpu_fiq=fiq)
    return intc, irq, fiq


def reg(intc, offset, value=None):
    if value is None:
        return intc.read("regs", offset, 4)
    intc.write("regs", offset, 4, value)


class IntcTest(unittest.TestCase):
    def test_masked_source_does_not_reach_cpu(self):
        intc, irq, _ = make()
        reg(intc, GLOBAL_EN, GLOBAL_IRQ)
        intc.line(UART2).set(True)
        self.assertFalse(irq.level)
        self.assertEqual(reg(intc, RAW), 1 << UART2)
        self.assertEqual(reg(intc, STATUS), 0)

    def test_enabled_source_raises_cpu_irq_when_global_irq_enabled(self):
        intc, irq, _ = make()
        reg(intc, IRQ_ENABLE, 1 << UART2)
        intc.line(UART2).set(True)
        self.assertFalse(irq.level)
        reg(intc, GLOBAL_EN, GLOBAL_IRQ)
        self.assertTrue(irq.level)
        self.assertEqual(reg(intc, STATUS), 1 << UART2)

    def test_write_one_clears_latched_status_once_cause_is_gone(self):
        intc, irq, _ = make()
        reg(intc, IRQ_ENABLE, 1 << UART2)
        reg(intc, GLOBAL_EN, GLOBAL_IRQ)
        line = intc.line(UART2)
        line.set(True)
        line.set(False)
        self.assertTrue(irq.level)
        reg(intc, STATUS, 1 << UART2)
        self.assertFalse(irq.level)
        self.assertEqual(reg(intc, RAW), 0)

    def test_write_one_does_not_clear_while_cause_persists(self):
        intc, irq, _ = make()
        reg(intc, IRQ_ENABLE, 1 << UART2)
        reg(intc, GLOBAL_EN, GLOBAL_IRQ)
        line = intc.line(UART2)
        line.set(True)
        reg(intc, RAW, 1 << UART2)
        self.assertTrue(irq.level)
        line.set(False)
        reg(intc, RAW, 1 << UART2)
        self.assertFalse(irq.level)

    def test_fiq_routing_follows_fiq_enable_bit(self):
        intc, irq, fiq = make()
        reg(intc, IRQ_ENABLE, 1 << FIQ_SOURCE)
        reg(intc, GLOBAL_EN, GLOBAL_IRQ)
        intc.line(FIQ_SOURCE).set(True)
        self.assertFalse(fiq.level)
        self.assertFalse(irq.level)
        reg(intc, GLOBAL_EN, GLOBAL_IRQ | GLOBAL_FIQ)
        self.assertTrue(fiq.level)
        self.assertFalse(irq.level)

    def test_two_lines_on_one_source_do_not_override_each_other(self):
        intc, irq, _ = make()
        reg(intc, IRQ_ENABLE, 1 << UART2)
        reg(intc, GLOBAL_EN, GLOBAL_IRQ)
        first, second = intc.line(UART2), intc.line(UART2)
        first.set(True)
        second.set(False)
        reg(intc, STATUS, 1 << UART2)  # drops the latch; `first` still asserts
        self.assertTrue(irq.level)
        first.set(False)
        reg(intc, STATUS, 1 << UART2)
        self.assertFalse(irq.level)

    def test_other_registers_read_back(self):
        intc, _, _ = make()
        reg(intc, 0x00, 0x1234)
        self.assertEqual(reg(intc, 0x00), 0x1234)


if __name__ == "__main__":
    unittest.main()
