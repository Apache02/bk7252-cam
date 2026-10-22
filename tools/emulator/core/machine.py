"""Assembles CPU, bus, scheduler and modules from a profile."""

from core.bus import Bus
from core.cpu import Cpu
from core.scheduler import Scheduler


class _CpuLine:
    def __init__(self, setter):
        self._setter = setter

    def set(self, level):
        self._setter(level)


class Machine:
    def __init__(self, profile, registry, endpoints):
        self.profile = profile
        self.cpu = Cpu()
        self.bus = Bus(self.cpu)
        self.modules = {}
        self.scheduler = Scheduler(self.cpu, [], profile.insns_per_us)
        self._cpu_irq = _CpuLine(self.cpu.raise_irq)
        self._cpu_fiq = _CpuLine(self.cpu.raise_fiq)

        for region in profile.memory:
            self.bus.add_memory(region.name, region.base, region.size)
        for spec in profile.modules:
            self._add_module(spec, registry, endpoints)
        self.bus.claim_unclaimed(*profile.peripheral_window)
        self.bus.set_fault_sink(lambda fault: self.cpu.post_fault(fault.kind))

    def _add_module(self, spec, registry, endpoints):
        if spec.type not in registry:
            raise ValueError(
                "unknown module type %r (known: %s)"
                % (spec.type, ", ".join(sorted(registry)))
            )
        params = dict(spec.params)
        params.update(
            clock=self.scheduler,
            cpu_irq=self._cpu_irq,
            cpu_fiq=self._cpu_fiq,
            reset_chip=self.reset,
        )
        if spec.name in endpoints:
            params["endpoint"] = endpoints[spec.name]
        if "intc" in params:
            controller = self._earlier(spec, params.pop("intc"))
            params["irq"] = controller.line(params["irq"])
        for key in [k for k in params if k.endswith("_signal")]:
            owner, _, signal = params.pop(key).partition(".")
            params[key[: -len("_signal")]] = self._earlier(spec, owner).signal(signal)
        module = registry[spec.type](spec.name, **params)
        self.bus.add_module(module, spec.bases)
        self.modules[spec.name] = module
        self.scheduler.modules.append(module)

    def _earlier(self, spec, name):
        module = self.modules.get(name)
        if module is None:
            raise ValueError(
                "module %s needs %s listed before it in the profile" % (spec.name, name)
            )
        return module

    def reset(self):
        """Chip reset: regions with a reset_fill are refilled, every module resets."""
        for region in self.profile.memory:
            if region.reset_fill is not None:
                self.cpu.mem_write(region.base, bytes([region.reset_fill]) * region.size)
        for module in self.modules.values():
            module.reset()
        self.cpu.reset(0)
