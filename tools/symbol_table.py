#!/usr/bin/env python3
"""
symbol_table.py - collect symbol table from .a static libraries
Usage: python3 symbol_table.py [directory or list of .a files]
Example:  python3 symbol_table.py libs/
          python3 symbol_table.py build/libs/*.a

Output CSV columns: lib, object, symbol, type, sym_type, size, section
  type: export or import (U = undefined/external reference)

Requirements: arm-none-eabi-nm (or nm) in PATH
"""

import subprocess
import sys
import os
import csv
from pathlib import Path

# Prefer arm-none-eabi-nm for ARM binaries
NM = "arm-none-eabi-nm" if subprocess.run(
    ["which", "arm-none-eabi-nm"], capture_output=True
).returncode == 0 else "nm"

OUTPUT = os.environ.get("SYMBOL_TABLE_OUTPUT", "symbol_table.csv")


def find_archives(args):
    archives = []
    for arg in args:
        p = Path(arg)
        if p.is_dir():
            archives.extend(sorted(p.rglob("*.a")))
        else:
            archives.append(p)
    return archives


def parse_archive(archive: Path):
    result = subprocess.run(
        [NM, "--format=sysv", "-A", str(archive)],
        capture_output=True, text=True
    )
    rows = []
    lib = archive.name
    for line in result.stdout.splitlines():
        # Skip headers and empty lines
        if not line or "Symbols from" in line or line.strip().startswith("Name"):
            continue
        # sysv format columns separated by '|':
        # lib:obj:SYMBOL  | VALUE | CLASS | TYPE | SIZE | LINE | SECTION
        parts = line.split("|")
        if len(parts) < 7:
            continue
        id_part      = parts[0]
        class_part   = parts[2].strip()
        type_part    = parts[3].strip()
        size_part    = parts[4].strip()
        size_part    = str(int(size_part, 16)) if size_part else ""
        section_part = parts[6].strip()
        # id_part format: "libfoo.a:bar.o:SYMBOL_NAME   "
        colon_parts = id_part.split(":")
        if len(colon_parts) < 3:
            continue
        obj = colon_parts[1].strip()
        sym = colon_parts[2].strip()
        # Skip ARM mapping symbols ($t, $d, $a) and empty names
        if not sym or sym.startswith("$"):
            continue
        kind = "import" if class_part == "U" else "export"
        rows.append((lib, obj, sym, kind, type_part, size_part, section_part))
    return rows


def main():
    args = sys.argv[1:] if sys.argv[1:] else ["."]
    archives = find_archives(args)

    if not archives:
        print("No .a files found", file=sys.stderr)
        sys.exit(1)

    print(f"Found {len(archives)} archive(s). Processing...", file=sys.stderr)

    with open(OUTPUT, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["lib", "object", "symbol", "type", "sym_type", "size", "section"])
        total = 0
        for archive in archives:
            rows = parse_archive(archive)
            writer.writerows(rows)
            total += len(rows)
            print(f"  {archive.name}: {len(rows)} symbols", file=sys.stderr)

    print(f"Done: {total} symbols -> {OUTPUT}", file=sys.stderr)


if __name__ == "__main__":
    main()