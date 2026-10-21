"""Memory map: RAM goes straight into Unicorn, modules are dispatched by address."""

import logging

from unicorn import (
    UC_HOOK_CODE,
    UC_HOOK_MEM_FETCH_UNMAPPED,
    UC_HOOK_MEM_READ,
    UC_HOOK_MEM_READ_UNMAPPED,
    UC_HOOK_MEM_WRITE_PROT,
    UC_HOOK_MEM_WRITE_UNMAPPED,
    UC_MEM_FETCH_UNMAPPED,
    UC_PROT_EXEC,
    UC_PROT_READ,
)

from core.module import Fault

PAGE = 0x1000
log = logging.getLogger("emulator.bus")


def _overlaps(spans, start, size):
    return any(start < end and s < start + size for s, end in spans)


class Bus:
    def __init__(self, cpu):
        self._cpu = cpu
        self._memory = []  # (name, base, size)
        self._claims = []  # (module, range_name, base, size)
        self._window = None
        self._sink = None
        self._reported = set()
        self._demand = []  # (module, range_name, base, size)
        self._demand_pages = {}  # page address -> hook handles
        self._warned_writes = set()
        self._bus_errors = set()

    def memory_regions(self):
        return list(self._memory)

    def module_ranges(self):
        return list(self._claims)

    def write_memory(self, address, data):
        self._cpu.mem_write(address, data)

    def set_fault_sink(self, fn):
        self._sink = fn

    def add_memory(self, name, base, size):
        self._check_free("memory " + name, base, size)
        self._cpu.map_ram(base, size)
        self._memory.append((name, base, size))

    def add_module(self, module, bases):
        for range_name, size in module.ranges():
            base = bases[range_name]
            label = "%s.%s" % (module.name, range_name)
            self._check_free(label, base, size)
            self._claims.append((module, range_name, base, size))
            if range_name in module.demand_ranges:
                self._demand.append((module, range_name, base, size))
                module.invalidate = self._invalidator(module)

    def claim_unclaimed(self, base, size):
        """Map the peripheral window; claimed modules are dispatched inside it."""
        self._window = (base, size)
        self._cpu.unicorn.mmio_map(
            base, size, self._window_read, None, self._window_write, None
        )
        for module, range_name, mbase, msize in self._claims:
            if (module, range_name, mbase, msize) in self._demand:
                self._install_demand(mbase, msize)
            elif not self._in_window(mbase, msize):
                self._map_own_region(module, range_name, mbase, msize)

        # Registered last so demand ranges get the first chance to map a page.
        self._cpu.unicorn.hook_add(
            UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED,
            self._on_bus_error,
        )

    def _on_bus_error(self, uc, access, addr, size, value, _):
        """Nothing answers at this address: abort, as a bus error would."""
        fetch = access == UC_MEM_FETCH_UNMAPPED
        page = addr & ~(PAGE - 1)
        if page not in self._bus_errors:
            self._bus_errors.add(page)
            log.warning(
                "bus error: unmapped %s at 0x%08X",
                "fetch" if fetch else "access",
                addr,
            )
        self._deliver(Fault("pabt" if fetch else "dabt", addr))
        return False

    def _install_demand(self, base, size):
        uc = self._cpu.unicorn
        end = base + size - 1
        uc.hook_add(
            UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED,
            self._on_unmapped, None, base, end,
        )
        uc.hook_add(UC_HOOK_MEM_WRITE_PROT, self._on_write_protected, None, base, end)

    def _find_demand(self, addr):
        for module, range_name, base, size in self._demand:
            if base <= addr < base + size:
                return module, range_name, base
        raise LookupError("no demand range at 0x%08X" % addr)

    def _on_unmapped(self, uc, access, addr, size, value, _):
        module, range_name, base = self._find_demand(addr)
        page_off = (addr - base) & ~(PAGE - 1)
        data, bad = module.page(range_name, page_off)
        if _overlaps(bad, addr - base - page_off, size):
            self._abort(module, range_name, addr - base, size, access == UC_MEM_FETCH_UNMAPPED)
            return False
        page = base + page_off
        uc.mem_map(page, PAGE, UC_PROT_READ | UC_PROT_EXEC)
        uc.mem_write(page, bytes(data))
        handles = []
        if bad:
            ctx = (module, range_name, base, page_off, bad)
            handles.append(uc.hook_add(UC_HOOK_MEM_READ, self._on_page_read, ctx, page, page + PAGE - 1))
            handles.append(uc.hook_add(UC_HOOK_CODE, self._on_page_code, ctx, page, page + PAGE - 1))
        self._demand_pages[page] = handles
        if handles:
            # Hooks added inside a callback only count once the access is re-executed
            # from scratch; a plain retry reuses a TLB entry made before they existed.
            self._cpu.request_restart()
        return True

    def _on_page_read(self, uc, access, addr, size, value, ctx):
        module, range_name, base, page_off, bad = ctx
        if _overlaps(bad, addr - base - page_off, size):
            self._abort(module, range_name, addr - base, size, False)

    def _on_page_code(self, uc, addr, size, ctx):
        module, range_name, base, page_off, bad = ctx
        if _overlaps(bad, addr - base - page_off, size):
            self._abort(module, range_name, addr - base, size, True)

    def _on_write_protected(self, uc, access, addr, size, value, _):
        module, range_name, base = self._find_demand(addr)
        if (module, range_name) not in self._warned_writes:
            self._warned_writes.add((module, range_name))
            log.warning("write to read-only %s.%s ignored at 0x%08X", module.name, range_name, addr)
        return True

    def _abort(self, module, range_name, offset, size, fetch):
        """The module's read() logs the details and raises the Fault."""
        kind = "pabt" if fetch else "dabt"
        try:
            module.read(range_name, offset, size)
        except Fault as fault:
            fault.kind = kind
            self._deliver(fault)
        else:
            self._deliver(Fault(kind, offset))

    def _deliver(self, fault):
        if self._sink is not None:
            self._sink(fault)

    def _invalidator(self, module):
        def invalidate(range_name, offset, size):
            for m, name, base, _ in self._demand:
                if m is module and name == range_name:
                    self._drop_pages(base + offset, size)
        return invalidate

    def _drop_pages(self, start, size):
        uc = self._cpu.unicorn
        for page in [p for p in self._demand_pages if start <= p < start + size]:
            for handle in self._demand_pages.pop(page):
                uc.hook_del(handle)
            uc.mem_unmap(page, PAGE)

    def _check_free(self, label, base, size):
        for name, mbase, msize in self._memory:
            if base < mbase + msize and mbase < base + size:
                raise ValueError("%s overlaps memory %s" % (label, name))
        for module, range_name, mbase, msize in self._claims:
            if base < mbase + msize and mbase < base + size:
                raise ValueError(
                    "%s overlaps %s.%s" % (label, module.name, range_name)
                )

    def _in_window(self, base, size):
        wbase, wsize = self._window
        return wbase <= base and base + size <= wbase + wsize

    def _map_own_region(self, module, range_name, base, size):
        if base % PAGE or size % PAGE:
            raise ValueError(
                "%s.%s is outside the peripheral window and not page aligned"
                % (module.name, range_name)
            )

        def read(uc, offset, rsize, _):
            return self._call(module.read, range_name, offset, rsize)

        def write(uc, offset, rsize, value, _):
            self._call(module.write, range_name, offset, rsize, value)

        self._cpu.unicorn.mmio_map(base, size, read, None, write, None)

    def _find(self, addr):
        for module, range_name, base, size in self._claims:
            if base <= addr < base + size:
                return module, range_name, addr - base
        return None

    def _window_read(self, uc, offset, size, _):
        addr = self._window[0] + offset
        found = self._find(addr)
        if found is None:
            self._report(addr, "read")
            return 0
        module, range_name, rel = found
        return self._call(module.read, range_name, rel, size)

    def _window_write(self, uc, offset, size, value, _):
        addr = self._window[0] + offset
        found = self._find(addr)
        if found is None:
            self._report(addr, "write")
            return
        module, range_name, rel = found
        self._call(module.write, range_name, rel, size, value)

    def _report(self, addr, what):
        if addr not in self._reported:
            self._reported.add(addr)
            log.warning("unclaimed peripheral %s at 0x%08X", what, addr)

    def _call(self, fn, *args):
        try:
            result = fn(*args)
        except Fault as fault:
            if self._sink is not None:
                self._sink(fault)
            return 0
        return 0 if result is None else result
