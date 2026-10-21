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
        params.update(clock=self.scheduler, cpu_irq=self._cpu_irq, cpu_fiq=self._cpu_fiq)
        if spec.name in endpoints:
            params["endpoint"] = endpoints[spec.name]
        if "intc" in params:
            intc_name = params.pop("intc")
            controller = self.modules.get(intc_name)
            if controller is None:
                raise ValueError(
                    "module %s needs %s listed before it in the profile"
                    % (spec.name, intc_name)
                )
            params["irq"] = controller.line(params["irq"])
        module = registry[spec.type](spec.name, **params)
        self.bus.add_module(module, spec.bases)
        self.modules[spec.name] = module
        self.scheduler.modules.append(module)
