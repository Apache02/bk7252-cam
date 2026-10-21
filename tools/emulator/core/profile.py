"""SoC profile: memory regions, load aliases and module instances, from TOML."""

import tomllib
from dataclasses import dataclass, field
from pathlib import Path


class ProfileError(ValueError):
    pass


@dataclass
class MemorySpec:
    name: str
    base: int
    size: int


@dataclass
class AliasSpec:
    name: str
    address: int | None = None
    module: str | None = None
    offset: int | None = None


@dataclass
class ModuleSpec:
    type: str
    name: str
    bases: dict
    params: dict = field(default_factory=dict)


@dataclass
class Profile:
    name: str
    insns_per_us: int
    peripheral_window: tuple
    memory: list
    aliases: dict  # upper-cased alias name -> AliasSpec
    modules: list

    def alias(self, name):
        return self.aliases.get(name.upper())


def available(directory):
    return sorted(p.stem for p in Path(directory).glob("*.toml"))


def load_profile(name, directory):
    if name != name.upper():
        raise ProfileError("profile name must be upper case: %r" % name)
    path = Path(directory) / (name + ".toml")
    if not path.is_file():
        raise ProfileError(
            "no profile %s (available: %s)" % (name, ", ".join(available(directory)))
        )
    try:
        raw = tomllib.loads(path.read_text())
        return _build(name, raw)
    except (KeyError, TypeError, tomllib.TOMLDecodeError) as err:
        raise ProfileError("%s: bad profile: %r" % (path.name, err)) from err


def _build(name, raw):
    memory = [MemorySpec(m["name"], m["base"], m["size"]) for m in raw["memory"]]
    modules = [
        ModuleSpec(m["type"], m["name"], dict(m["bases"]), dict(m.get("params", {})))
        for m in raw.get("module", [])
    ]
    module_names = {m.name for m in modules}
    aliases = {}
    for key, value in raw.get("alias", {}).items():
        alias = _alias(key, value)
        if alias.name in aliases:
            raise ProfileError("duplicate alias %s" % key)
        if alias.module is not None:
            if alias.module not in module_names:
                raise ProfileError("alias %s names unknown module %s" % (key, alias.module))
        elif not any(m.base <= alias.address < m.base + m.size for m in memory):
            raise ProfileError("alias %s is outside memory: 0x%X" % (key, alias.address))
        aliases[alias.name] = alias
    return Profile(
        name=raw["name"],
        insns_per_us=raw["insns_per_us"],
        peripheral_window=tuple(raw["peripheral_window"]),
        memory=memory,
        aliases=aliases,
        modules=modules,
    )


def _alias(key, value):
    if isinstance(value, int):
        return AliasSpec(key.upper(), address=value)
    return AliasSpec(key.upper(), module=value["module"], offset=value["offset"])
