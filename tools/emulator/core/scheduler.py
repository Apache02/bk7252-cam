"""Device clock: the CPU runs in slices, modules tick between slices."""

from core.cpu import RunResult


class Scheduler:
    def __init__(self, cpu, modules, insns_per_us, slice_insns=1000):
        self._cpu = cpu
        self.modules = modules
        self._insns_per_us = insns_per_us
        self._slice_insns = slice_insns
        self._now_ns = 0
        self.stopped = False

    def now_ns(self):
        return self._now_ns

    def step(self):
        result, executed = self._cpu.run(self._slice_insns)
        if result is RunResult.STOPPED:
            self.stopped = True
            return
        if result is RunResult.IDLE:
            self._now_ns = max(self._now_ns, self._idle_target())
        else:
            self._now_ns += executed * 1000 // self._insns_per_us
        for module in self.modules:
            module.tick(self._now_ns)

    def run_forever(self, should_stop):
        while not self.stopped and not should_stop():
            self.step()

    def _idle_target(self):
        events = [e for e in (m.next_event_ns() for m in self.modules) if e is not None]
        later = [e for e in events if e > self._now_ns]
        if later:
            return min(later)
        return self._now_ns + self._slice_insns * 1000 // self._insns_per_us
