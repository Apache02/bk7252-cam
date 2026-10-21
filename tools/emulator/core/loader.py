"""--load handling: resolve targets, validate every load, then apply them."""

from dataclasses import dataclass
from pathlib import Path


class LoadError(ValueError):
    pass


@dataclass
class LoadSpec:
    target: str
    path: Path


def parse_load(arg):
    target, sep, path = arg.partition(":")
    if not sep or not target or not path:
        raise LoadError("--load expects target:file, got %r" % arg)
    return LoadSpec(target, Path(path))


def apply_loads(specs, profile, bus, cpu, modules):
    actions = [_plan(spec, profile, bus, modules) for spec in specs]
    for action in actions:
        action()


def _plan(spec, profile, bus, modules):
    try:
        data = spec.path.read_bytes()
    except OSError as err:
        raise LoadError("cannot read %s: %s" % (spec.path, err.strerror)) from err
    if not data:
        raise LoadError("%s is empty" % spec.path)

    alias = profile.alias(spec.target)
    if alias is not None and alias.module is not None:
        module = modules[alias.module]
        if not module.loadable:
            raise LoadError("%s cannot be loaded" % alias.module)
        try:
            module.check_load(alias.offset, len(data))
        except ValueError as err:
            raise LoadError("%s: %s" % (spec.path, err)) from err
        return lambda: module.load(alias.offset, data)

    address = alias.address if alias is not None else _number(spec.target)
    _check_memory(spec, address, len(data), bus)
    return lambda: bus.write_memory(address, data)


def _number(text):
    try:
        return int(text, 0)
    except ValueError:
        raise LoadError("unknown load target %r (not an alias or a number)" % text) from None


def _check_memory(spec, address, size, bus):
    end = address + size
    for module, range_name, base, rsize in bus.module_ranges():
        if address < base + rsize and base < end:
            raise LoadError(
                "%s: 0x%08X..0x%08X lies in %s.%s, which cannot be loaded"
                % (spec.path, address, end, module.name, range_name)
            )
    for _, base, rsize in bus.memory_regions():
        if base <= address < base + rsize:
            if end > base + rsize:
                raise LoadError(
                    "%s does not fit: 0x%08X + %d bytes crosses the end of the region"
                    % (spec.path, address, size)
                )
            return
    raise LoadError("%s: address 0x%08X is outside memory" % (spec.path, address))
