#!/usr/bin/env python3
"""
lib_symbols.py - analyze symbol imports/exports across .a archives and .o objects.

Both are read from the given directory, so a loose object dropped in next to the
archives is resolved against them — which is how you check whether a replacement
object still satisfies (and is satisfied by) the rest of the link.

Usage:
    python3 lib_symbols.py <lib_dir> [options]
    python3 lib_symbols.py <lib_dir> --link import --empty-import-from
    python3 lib_symbols.py <lib_dir> --file bk7251 --symbol rwnxl
    python3 lib_symbols.py <lib_dir> --csv > out.csv

Columns: file, object, symbol, type, link, size, import_from

  file         archive or object filename as found on disk
  object       archive member; for a standalone .o, the file itself
  import_from  For link=import: "file/object" of the matching export ("file"
               alone when the file is a standalone object).
               Empty means unresolved (nothing in the directory exports it).
               Always empty for link=export.

Filters (all substring, case-insensitive, except --link and --type which are exact):
  --file FILE             archive filename substring   (e.g. "bk7251")
  --object OBJ            object file name substring   (e.g. "rwnx.o")
  --symbol SYM            symbol name substring        (e.g. "rwnxl_init")
  --type TYPE             exact ELF type               (FUNC OBJECT NOTYPE ...)
  --link {import,export}  exact link direction
  --empty-import-from     only unresolved imports (link=import AND import_from empty)

Output:
  --csv     CSV to stdout instead of aligned text table
  --count   print only the row count
"""

import argparse
import csv
import os
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


# ---------------------------------------------------------------------------
# Tool detection
# ---------------------------------------------------------------------------

def _find_nm():
    for name in ("arm-none-eabi-nm", "nm"):
        if subprocess.run(["which", name], capture_output=True).returncode == 0:
            return name
    sys.exit("error: nm not found in PATH (install binutils or arm-none-eabi-binutils)")


NM = None  # resolved lazily


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

def _is_archive(path: Path) -> bool:
    """True for ar archives, by magic bytes — extensions are not trusted."""
    with path.open("rb") as f:
        return f.read(8) == b"!<arch>\n"


def _parse_binary(path: Path) -> list[dict]:
    """
    Run nm --format=sysv -A on *path* (an .a archive or a bare .o) and return a
    list of symbol dicts: file, object, symbol, type, link, size, import_from
    (always '' here)
    """
    global NM
    if NM is None:
        NM = _find_nm()

    result = subprocess.run(
        [NM, "--format=sysv", "-A", str(path)],
        capture_output=True, text=True,
    )

    rows = []
    lib_name = path.name
    is_archive = _is_archive(path)

    for line in result.stdout.splitlines():
        if not line or "Symbols from" in line or line.strip().startswith("Name"):
            continue

        # nm sysv -A format:
        #   /path/archive.a:obj.o:SYMBOL  |  value | class | type | size | line | section
        parts = line.split("|")
        if len(parts) < 7:
            continue

        id_part   = parts[0]
        cls       = parts[2].strip()   # U / T / D / B / W / ...
        sym_type  = parts[3].strip()   # FUNC / OBJECT / NOTYPE / ...
        size_hex  = parts[4].strip()
        # section = parts[6].strip()   # not used in output

        # Archive: "/full/path/archive.a:obj.o:SYMBOL   "
        # Object:   "/full/path/obj.o:SYMBOL   "
        # Split on ':' from the right to avoid issues with paths containing ':'
        colon_parts = id_part.split(":")
        if len(colon_parts) < (3 if is_archive else 2):
            continue
        obj = colon_parts[-2].strip() if is_archive else lib_name
        sym = colon_parts[-1].strip()

        if not sym or sym.startswith("$"):
            continue

        # Lowercase class = local symbol; skip
        if cls != "U" and not cls.isupper():
            continue

        link = "import" if cls == "U" else "export"
        size = str(int(size_hex, 16)) if size_hex else ""
        if link == "import":
            size = ""

        rows.append({
            "file":        lib_name,
            "object":      obj,
            "symbol":      sym,
            "type":        sym_type,
            "link":        link,
            "size":        size,
            "import_from": "",
        })

    return rows


def load_all(lib_dir: Path) -> list[dict]:
    inputs = sorted(lib_dir.glob("*.a")) + sorted(lib_dir.glob("*.o"))
    if not inputs:
        sys.exit(f"error: no .a or .o files found in {lib_dir}")

    all_rows = []
    for path in inputs:
        rows = _parse_binary(path)
        all_rows.extend(rows)
        print(f"  {path.name}: {len(rows)} symbols", file=sys.stderr)

    print(f"Total: {len(all_rows)} symbols across {len(inputs)} files",
          file=sys.stderr)
    return all_rows


# ---------------------------------------------------------------------------
# Import resolution
# ---------------------------------------------------------------------------

def resolve_imports(rows: list[dict]) -> None:
    """Fill import_from for every import row (in-place)."""
    export_index: dict[str, list[str]] = defaultdict(list)
    for r in rows:
        if r["link"] == "export":
            # A standalone object is its own member; don't print the name twice.
            label = r["file"] if r["file"] == r["object"] else f"{r['file']}/{r['object']}"
            export_index[r["symbol"]].append(label)

    for r in rows:
        if r["link"] == "import":
            providers = export_index.get(r["symbol"], [])
            r["import_from"] = ", ".join(providers)


# ---------------------------------------------------------------------------
# Filtering
# ---------------------------------------------------------------------------

def apply_filters(rows: list[dict], args: argparse.Namespace) -> list[dict]:
    result = rows

    if args.file:
        lo = args.file.lower()
        result = [r for r in result if lo in r["file"].lower()]
    if args.object:
        lo = args.object.lower()
        result = [r for r in result if lo in r["object"].lower()]
    if args.symbol:
        lo = args.symbol.lower()
        result = [r for r in result if lo in r["symbol"].lower()]
    if args.type:
        exact = args.type.upper()
        result = [r for r in result if r["type"].upper() == exact]
    if args.link:
        result = [r for r in result if r["link"] == args.link]
    if args.empty_import_from:
        result = [r for r in result
                  if r["link"] == "import" and not r["import_from"]]

    return result


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

COLUMNS = ["file", "object", "symbol", "type", "link", "size", "import_from"]


def print_table(rows: list[dict]) -> None:
    if not rows:
        print("(no results)")
        return

    widths = {col: len(col) for col in COLUMNS}
    for r in rows:
        for col in COLUMNS:
            widths[col] = max(widths[col], len(r.get(col, "")))

    fmt = "  ".join(f"{{:<{widths[col]}}}" for col in COLUMNS)
    sep = "  ".join("-" * widths[col] for col in COLUMNS)

    print(fmt.format(*COLUMNS))
    print(sep)
    for r in rows:
        print(fmt.format(*[r.get(col, "") for col in COLUMNS]))


def print_csv_output(rows: list[dict]) -> None:
    writer = csv.DictWriter(sys.stdout, fieldnames=COLUMNS, extrasaction="ignore")
    writer.writeheader()
    writer.writerows(rows)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("lib_dir", help="Directory containing .a archives and/or .o objects")

    g = parser.add_argument_group("filters")
    g.add_argument("--file",   metavar="SUBSTR", help="archive/object filename substring")
    g.add_argument("--object", metavar="SUBSTR", help="archive member name substring")
    g.add_argument("--symbol", metavar="SUBSTR", help="symbol name substring")
    g.add_argument("--type",   metavar="TYPE",   help="exact ELF type (FUNC, OBJECT, NOTYPE…)")
    g.add_argument("--link",   choices=["import", "export"], help="import or export")
    g.add_argument("--empty-import-from", dest="empty_import_from",
                   action="store_true",
                   help="only unresolved imports (link=import AND import_from empty)")

    out = parser.add_argument_group("output")
    out.add_argument("--csv",   action="store_true", help="CSV to stdout")
    out.add_argument("--count", action="store_true", help="print row count only")

    args = parser.parse_args()

    lib_dir = Path(args.lib_dir)
    if not lib_dir.is_dir():
        sys.exit(f"error: not a directory: {lib_dir}")

    rows = load_all(lib_dir)
    resolve_imports(rows)

    filtered = apply_filters(rows, args)

    if args.count:
        print(len(filtered))
        return

    if args.csv:
        print_csv_output(filtered)
    else:
        print_table(filtered)
        print(f"\n{len(filtered)} rows")


if __name__ == "__main__":
    main()
