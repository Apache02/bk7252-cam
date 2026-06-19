---
name: bk7252-register-headers
description: Use when generating a new MMIO register header for a BK7252 driver from vendor SDK source, or when explicitly asked to refactor an existing header in this project to the canonical style. Triggers on phrases like "generate a register header for <block>", "translate this SDK fragment into a register header", "make a register header from this", "bring `soc/<block>.h` to canon", "rewrite this SDK fragment to canon". Also use when the user pastes a Beken SDK fragment that contains `_POSI` / `_MASK` macros, `(1UL << N)` constants, and a base-address `#define` — that's the input shape.
---

# BK7252 Register Headers

Translate Beken vendor SDK register descriptions into the project's canonical MMIO header form, or bring an existing header into line with that canon when explicitly asked.

Output is a single C header that mirrors the on-chip memory layout via a `volatile struct` with bit-field unions. Behavioural documentation belongs in `docs/hardware/<block>.md`. Full rule definitions and code examples are in `docs/conventions/soc-register-headers.md`.

**REQUIRED SUB-SKILL:** Use `bk7252-sdk-reading` first — it covers chip identity, CFG_SOC_NAME branch resolution, SDK source layout, register encoding patterns (`_POSI`/`_MASK`), and SDK reliability. This skill covers only the output format.

## When this skill applies

**Generation** — user wants a new register header for `<block>` from SDK input.

- They paste an SDK fragment (typically `beken378/driver/<block>/<block>.h` or `<block>_reg.h`), or give a path.
- The block has no existing `soc/<block>.h` in `src/platform/soc/bk7221u/include/soc/`.

**Refactor** — user explicitly asks to bring an existing header to canon.

- Phrase like "bring `soc/<block>.h` to canon" / "fix `soc/<block>.h` style".
- Do NOT refactor existing headers opportunistically when editing for other reasons — refactor only on explicit ask.

## Output canon

File path: `src/platform/soc/bk7221u/include/soc/<block>.h`, included as `#include "soc/<block>.h"`.

```c
#pragma once

#include "platform/soc.h"      // pulls <stdint.h>, exposes hw_write_fields


#define <BLOCK>_BASE_ADDR    (0x00......)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t enable: 1;            // [0]    1 = block runs; hw clears at completion
            uint32_t mode: 2;              // [2:1]  0 = A, 1 = B, 2 = C
            uint32_t reserved_3_31: 29;    // [31:3]
        };
    } ctrl;

    uint32_t data;                         // plain data register
} hw_<block>_t;

#define hw_<block>      ((volatile hw_<block>_t *) <BLOCK>_BASE_ADDR)
```

### 12 conventions (quick reference — full detail in `docs/conventions/soc-register-headers.md`)

| # | Rule |
|---|------|
| 1 | Union layout: `uint32_t v;` first, struct second |
| 2 | Bit-range comments: `[hi:lo]` (MSB:LSB). Single bit: `// [N]`. Legacy `[lo:hi]` is wrong — fix on refactor. |
| 3 | Every bit-field gets an inline `// [bits]` comment; add semantic note when name isn't self-evident |
| 4 | Reserved naming: `reserved_<lo>_<hi>: N;` for ranges, `reserved_<n>: 1;` for single bits. Union must sum to 32. |
| 5 | `#include "platform/soc.h"` required |
| 6 | Typedef: `hw_<block>_t`; sub-engine: `hw_<block>_<sub>_t`. Legacy `*_reg_t` / `efuse_hw_t` / `icu_*_reg_t` → fix on refactor. |
| 7 | Base macro: `<BLOCK>_BASE_ADDR` standalone; `<PARENT>_BASE + N * 4` when inside a parent block |
| 8 | Pointer macro: `hw_<block>` singleton, `hw_<block>N` multi-instance. Goes at bottom of file. |
| 9 | Gaps: `uint32_t reserved_0x<offset>;` single word; `uint32_t reserved_0x<lo>_0x<hi>[N];` multi-word. No `_word_` infix. |
| 10 | Verification markers: `(NOT TESTED)` / `(NOT INVESTIGATED)` / `(from SDK)` / `(NOT FUNCTIONAL IN SILICON)`. Default: trust the SDK. Use sparingly, per-field only. |
| 11 | SDK quirks (typos, keyword collisions): inline comment explaining why the quirk stays |
| 12 | Blank-line spacing: 2 blank lines after last `#include`; 2 blank lines after last top `#define`; 1 blank line before pointer macro |

## Workflow — generation

1. Read the SDK fragment.
2. **Resolve `#if CFG_SOC_NAME`** using `bk7252-sdk-reading`. Keep only `SOC_BK7221U` branches.
3. **Identify base address and register offsets**. Build a flat list: `(offset, name, kind)` where `kind ∈ {bit-field register, data register, gap}`.
4. **Translate each bit-field register**:
    - Group `<REG>_<FIELD>_POSI` / `_MASK` pairs and `(1<<N)` flags.
    - Convert mask → width: `0x1`→1, `0x3`→2, `0x7`→3, `0xF`→4, `0x1F`→5, `0xFF`→8, `0x3FF`→10, `0xFFF`→12, `0xFFFF`→16.
    - Lay out fields in ascending `POSI` order inside `union { uint32_t v; struct { ... }; }`.
    - Fill unallocated ranges with `reserved_<lo>_<hi>` so every union totals 32.
    - Add `// [bits]` + semantic comment per field (rule 3).
5. **Translate data registers** as plain `uint32_t name;`.
6. **Insert gap padding** for non-contiguous offsets (rule 9).
7. **Apply naming**: `hw_<block>_t`, `hw_<block>`, `<BLOCK>_BASE_ADDR`. Rename only for C-keyword collisions (rule 11).
8. **Verification markers**: sparingly, per rule 10.
9. **Sanity checks**: every union sums to 32; every offset accounted for; file path is `soc/<block>.h`.

## Workflow — refactor existing

Same target canon. **Only run when explicitly asked.**

Common legacy deltas:

- `[lo:hi]` bit comments → flip to `[hi:lo]` (rule 2)
- Struct-first union → `v`-first union (rule 1)
- `<block>_hw_t` / `*_reg_t` / `icu_<...>_reg_t` → `hw_<block>_t` (rule 6) — grep callers first
- Operation-model block comments in header → remove (belongs in `docs/hardware/<block>.md`)
- Missing inline bit-comments → add per rule 3

## Real examples

- `soc/gdma.h` — canonical form; gap padding; multi-register channel struct
- `soc/security.h` — multi-engine block; `auteo` SDK-quirk comment
- `soc/random.h` — minimal canonical form
- `platform/soc.h` — required base include

## Final pass before delivering

- Is every bit-field's width and position obvious without consulting the SDK?
- Does the file declare exactly the hardware layout, nothing more?
- Are verification markers placed only where there's a real reason to doubt?
- Is the path `src/platform/soc/bk7221u/include/soc/<block>.h`?

If any answer is wrong, fix it before delivering.
