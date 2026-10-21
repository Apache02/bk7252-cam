import unittest

from core.cpu import RunResult
from core.module import Module
from core.scheduler import Scheduler


class FakeCpu:
    def __init__(self, *results):
        self.results = list(results)
        self.budgets = []

    def run(self, budget):
        self.budgets.append(budget)
        return self.results.pop(0)


class Ticker(Module):
    def __init__(self, name, log, event=None):
        super().__init__(name)
        self.log = log
        self.event = event

    def tick(self, now_ns):
        self.log.append((self.name, now_ns))

    def next_event_ns(self):
        return self.event


class SchedulerTest(unittest.TestCase):
    def test_clock_advances_by_executed_insns(self):
        cpu = FakeCpu((RunResult.BUDGET, 1000))
        sched = Scheduler(cpu, [], insns_per_us=100)
        sched.step()
        self.assertEqual(sched.now_ns(), 10_000)

    def test_idle_jumps_to_next_event(self):
        cpu = FakeCpu((RunResult.IDLE, 1))
        sched = Scheduler(cpu, [Ticker("T", [], event=5_000_000)], insns_per_us=100)
        sched.step()
        self.assertEqual(sched.now_ns(), 5_000_000)

    def test_idle_without_event_advances_one_slice(self):
        cpu = FakeCpu((RunResult.IDLE, 1))
        sched = Scheduler(cpu, [], insns_per_us=100, slice_insns=1000)
        sched.step()
        self.assertEqual(sched.now_ns(), 10_000)

    def test_past_event_does_not_move_clock_backwards(self):
        cpu = FakeCpu((RunResult.BUDGET, 1000), (RunResult.IDLE, 1))
        sched = Scheduler(cpu, [Ticker("T", [], event=1)], insns_per_us=100)
        sched.step()
        sched.step()
        self.assertEqual(sched.now_ns(), 20_000)

    def test_tick_called_every_slice_in_order(self):
        log = []
        cpu = FakeCpu((RunResult.BUDGET, 1000), (RunResult.BUDGET, 1000))
        sched = Scheduler(cpu, [Ticker("A", log), Ticker("B", log)], insns_per_us=100)
        sched.step()
        sched.step()
        self.assertEqual(
            log, [("A", 10_000), ("B", 10_000), ("A", 20_000), ("B", 20_000)]
        )

    def test_run_forever_stops_on_request(self):
        cpu = FakeCpu(*[(RunResult.BUDGET, 1000)] * 5)
        sched = Scheduler(cpu, [], insns_per_us=100)
        sched.run_forever(lambda: len(cpu.budgets) >= 3)
        self.assertEqual(len(cpu.budgets), 3)

    def test_run_forever_stops_when_cpu_stopped(self):
        cpu = FakeCpu((RunResult.STOPPED, 0))
        sched = Scheduler(cpu, [], insns_per_us=100)
        sched.run_forever(lambda: False)
        self.assertEqual(len(cpu.budgets), 1)


if __name__ == "__main__":
    unittest.main()
