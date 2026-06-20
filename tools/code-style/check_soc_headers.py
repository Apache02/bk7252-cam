#!/usr/bin/env python3
"""Validate soc register headers against the project's canonical style.

See docs/conventions/soc-register-headers.md for the full rule set.

Checks performed:
  R1  - Union layout: uint32_t v first, struct second
  R2  - Bit-range comments [hi:lo] (hi >= lo); single-bit [N] is fine
  R3  - (not checked: // [bits] comment is optional)
  R4  - Reserved bit-fields named reserved_<lo>_<hi> or reserved_<n>
  R5  - #include "platform/soc.h" present
  R6  - No legacy _reg_t typedef suffix
  R9  - Gap arrays named reserved_0x<lo>_0x<hi>, no _word_ infix
  R12 - 2 blank lines after last #include; 2 blank lines after last top #define

Not checked (require judgment):
  R7  - Base macro naming
  R8  - Pointer macro placement
  R10 - Verification markers used appropriately
  R11 - SDK quirks documented inline

Usage:
  python3 tools/code-style/check_soc_headers.py            # all soc/*.h
  python3 tools/code-style/check_soc_headers.py path/to/file.h ...
"""

import re
import sys
import glob
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SOC_DIR = ROOT / 'src/platform/soc/bk7221u/include/soc'

# Matches a bit-field declaration line, capturing type, name, width.
# Intentionally broad — matches indented lines with `identifier: digits`.
_BITFIELD_RE = re.compile(
    r'^(?P<indent>\s+)'
    r'(?P<type>\w[\w\s]*?)\s+'
    r'(?P<name>\w+)\s*:\s*(?P<width>\d+)\s*;'
    r'(?P<rest>.*)$'
)

# Matches an inline bit-range comment: // [N] or // [hi:lo]
_BITCOMMENT_RE = re.compile(r'//[^\[]*\[(\d+)(?::(\d+))?\]')


def check(path: Path) -> list[str]:
    issues = []
    text = path.read_text()
    lines = text.splitlines(keepends=True)

    def fail(rule, lineno, msg):
        issues.append(f'R{rule}:{lineno}: {msg}')

    # ------------------------------------------------------------------ R5
    if '#include "platform/soc.h"' not in text:
        fail(5, 0, 'missing #include "platform/soc.h"')

    # ------------------------------------------------------------------ R6
    for i, line in enumerate(lines, 1):
        if re.match(r'^\s*\}\s+\w+_reg_t\s*;', line):
            fail(6, i, f'legacy _reg_t typedef: {line.strip()}')

    # ------------------------------------------------------------------ R9
    for i, line in enumerate(lines, 1):
        # Gap arrays: uint32_t reserved_word_… or similar _word_ pattern
        if re.search(r'\breserved\w*_word_', line):
            fail(9, i, f'_word_ infix in gap name: {line.strip()}')
        # Gap arrays should be reserved_0x<lo>_0x<hi> (hex offsets)
        m = re.match(r'\s+uint32_t\s+(reserved_\w+)\s*\[\d+\]\s*;', line)
        if m and not re.match(r'^reserved_0x[0-9a-fA-F]+_0x[0-9a-fA-F]+$', m.group(1)):
            fail(9, i, f'gap array name not reserved_0x<lo>_0x<hi>: {m.group(1)}')

    # ------------------------------------------------------------------ R12
    last_inc = max(
        (i for i, l in enumerate(lines) if l.startswith('#include')),
        default=-1,
    )
    first_def_after_inc = next(
        (i for i in range(last_inc + 1, len(lines)) if lines[i].startswith('#define')),
        -1,
    )
    first_typedef = next(
        (i for i, l in enumerate(lines)
         if 'typedef volatile struct' in l or 'typedef struct' in l),
        -1,
    )
    last_def_before_typedef = -1
    if first_typedef != -1:
        for i in range(first_typedef - 1, -1, -1):
            if lines[i].startswith('#define'):
                last_def_before_typedef = i
                break

    if last_inc >= 0 and first_def_after_inc > last_inc:
        gap = [l.strip() for l in lines[last_inc + 1:first_def_after_inc]]
        if gap != ['', '']:
            fail(12, last_inc + 1, f'expected 2 blank lines after #include block, got {gap!r}')

    if last_def_before_typedef >= 0 and first_typedef > last_def_before_typedef:
        gap = [l.strip() for l in lines[last_def_before_typedef + 1:first_typedef]]
        # typedef enum blocks are allowed between #define and typedef volatile struct;
        # check only the immediate spacing on each side.
        if gap[:2] != ['', '']:
            fail(12, last_def_before_typedef + 1,
                 f'expected 2 blank lines after #define block, got {gap[:4]!r}')
        if gap[-2:] != ['', '']:
            fail(12, first_typedef,
                 f'expected 2 blank lines before typedef volatile struct, got {gap[-4:]!r}')

    # ------------------------------------------------------------------ R1
    for m in re.finditer(r'union\s*\{', text):
        # Scan forward for the first 'struct' and first 'uint32_t v;'
        block = text[m.start():m.start() + 500]
        si = block.find('struct')
        vi = block.find('uint32_t v;')
        if si != -1 and vi != -1 and si < vi:
            lineno = text[:m.start()].count('\n') + 1
            fail(1, lineno, 'struct member appears before uint32_t v in union')

    # ------------------------------------------------------------------ R2 / R3
    # Track whether we are inside a union struct body to identify bit-fields.
    # Use a simple brace-depth approach: bit-field lines are those matched by
    # _BITFIELD_RE that appear inside a struct { } body inside a union { }.
    union_depth = 0      # depth inside `union {`
    struct_depth = 0     # depth inside `struct {` within current union
    brace_depth = 0

    for i, raw in enumerate(lines, 1):
        line = raw.rstrip('\n')

        # Count braces to track nesting
        opens = line.count('{')
        closes = line.count('}')

        m = _BITFIELD_RE.match(line)
        if m:
            name = m.group('name')
            width = int(m.group('width'))
            rest = m.group('rest')

            # R2: if a [hi:lo] comment is present, check ordering
            cm = _BITCOMMENT_RE.search(rest)
            if cm is not None and cm.group(2) is not None:  # two-part range [X:Y]
                hi, lo = int(cm.group(1)), int(cm.group(2))
                if hi < lo:
                    fail(2, i, f'bit-range comment is lo:hi, should be hi:lo: [{hi}:{lo}]')

            # R4: reserved fields must follow reserved_<lo>_<hi> or reserved_<n>
            if name.startswith('reserved'):
                if not re.match(r'^reserved(_\d+){1,2}$', name):
                    fail(4, i, f'non-canonical reserved field name: {name}')

        brace_depth += opens - closes

    return issues


def main(paths: list[Path]) -> int:
    total_issues = 0
    fail_count = 0

    for path in sorted(paths):
        issues = check(path)
        name = path.name
        if issues:
            fail_count += 1
            total_issues += len(issues)
            print(f'FAIL {name}')
            for iss in issues:
                print(f'     {iss}')
        else:
            print(f'ok   {name}')

    print()
    if total_issues == 0:
        print(f'All {len(paths)} file(s) pass.')
        return 0
    else:
        print(f'{total_issues} issue(s) in {fail_count}/{len(paths)} file(s).')
        return 1


if __name__ == '__main__':
    if len(sys.argv) > 1:
        paths = [Path(p) for p in sys.argv[1:]]
    else:
        paths = [Path(p) for p in sorted(glob.glob(str(SOC_DIR / '*.h')))]

    if not paths:
        print(f'No files found in {SOC_DIR}', file=sys.stderr)
        sys.exit(1)

    sys.exit(main(paths))
