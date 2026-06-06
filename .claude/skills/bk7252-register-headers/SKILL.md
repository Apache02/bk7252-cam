---
name: bk7252-register-headers
description: Use when generating a new `<block>_regs.h` for a BK7252 driver from vendor SDK source, or when explicitly asked to refactor an existing `*_regs.h` in this project to the canonical style. Triggers on phrases like "generate `<block>_regs.h`", "translate this SDK fragment into a regs header", "make a regs header from this", "bring `<file>_regs.h` to canon", "rewrite this SDK fragment to canon". Also use when the user pastes a Beken SDK fragment that contains `_POSI` / `_MASK` macros, `(1UL << N)` constants, and a base-address `#define` — that's the input shape.
---

# BK7252 Register Headers

Translate Beken vendor SDK register descriptions into the project's canonical `*_regs.h` form, or bring an existing `*_regs.h` into line with that canon when explicitly asked.

Output is a single C header that mirrors the on-chip memory layout via a `volatile struct` with bit-field unions. Behavioural documentation — operation model, edge cases, transfer invariants — does NOT live in this file; it goes in `docs/hardware/<block>.md` (see the `hardware-block-reference` skill).

**REQUIRED SUB-SKILL:** Use `bk7252-sdk-reading` first — it covers chip identity, CFG_SOC_NAME branch resolution, SDK source layout, register encoding patterns (`_POSI`/`_MASK`), and SDK reliability. This skill covers only the output format.

## When this skill applies

Two distinct triggers; the workflow differs.

**Generation** — user wants a new `<block>_regs.h` from SDK input.

- They paste an SDK fragment (typically `beken378/driver/<block>/<block>.h` or `<block>_reg.h`), or give a path.
- The block has no existing `*_regs.h` in `src/platform/drivers/hardware_<block>/`.

**Refactor** — user explicitly asks to bring an existing `*_regs.h` to canon.

- Phrase like "bring `<file>_regs.h` to canon" / "fix `<file>_regs.h` style".
- Do NOT refactor existing `*_regs.h` opportunistically when editing it for other reasons — refactor only on explicit ask.

## Output canon

### File skeleton

```c
#pragma once

#include "register_defs.h"      // pulls <stdint.h>, exposes hw_write_fields

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

    // ... other registers
} hw_<block>_t;

#define hw_<block>      ((volatile hw_<block>_t *) <BLOCK>_BASE_ADDR)
```

### 11 conventions

| # | Rule | Notes |
|---|---|---|
| 1 | Union layout: `uint32_t v;` first, struct second | `v` first matches majority of existing files and makes the alias discoverable on first scan. |
| 2 | Bit-range comments: `[hi:lo]` (MSB:LSB) | Matches industry/datasheet convention and `docs/hardware/*.md`. Legacy files using `[lo:hi]` are wrong; bring them to MSB:LSB on refactor. |
| 3 | Every bit-field gets an inline `// [bits]` placement comment AND a semantic comment when meaning isn't fully self-evident from the name | Apply to single-bit fields too — placement is cheap, future readers benefit. |
| 4 | Reserved bit-field naming: `reserved_<lo>_<hi>: N;` for ranges, `reserved_<n>: 1;` for single bits | All bit-fields in a union must sum to exactly 32. |
| 5 | Includes: `#include "register_defs.h"` is required. Additional standard headers are allowed when needed. | — |
| 6 | Typedef name: `hw_<block>_t` for a memory-mapped block; `hw_<block>_<sub>_t` for sub-engines inside a parent block (e.g. `hw_security_aes_reg_t` — but use `hw_<block>_<sub>_t` for new code; the `_reg_t` suffix in `security_regs.h` is legacy) | Existing `efuse_hw_t`, `*_reg_t`, `icu_<...>_reg_t` are legacy — bring to `hw_<block>_t` on refactor. |
| 7 | Base macro: `<BLOCK>_BASE_ADDR` for a standalone block; `<BLOCK>_BASE + N * 4` when the block sits inside a parent (as `EFUSE_BASE` does inside `SCTRL_BASE`) | Pattern matches gdma/random/uart/gpio. |
| 8 | Pointer macro: `hw_<block>` for singletons; `hw_<block>N` for multi-instance (e.g. `hw_uart1`, `hw_uart2`) | Goes at the bottom of the file, after the typedef. |
| 9 | Gap naming between registers: `uint32_t reserved_0x<offset>;` for a single-word gap; `uint32_t reserved_0x<lo>_0x<hi>[N];` for a multi-word gap | Always specify the offset in the name. Word-based (no `_word_` infix). |
| 10 | Verification markers: `(NOT TESTED)` — semantics from SDK, behaviour not verified on hardware; `(NOT INVESTIGATED)` — never looked at; `(from SDK)` — copied verbatim with no validation; `(NOT FUNCTIONAL IN SILICON)` — SDK describes it but the silicon doesn't honour the description | Use sparingly and per-field. **Default position: trust the SDK** — most of it has been validated by real firmware running on this chip. Apply markers to specific fields that this project has not exercised, not to whole blocks. |
| 11 | SDK quirks (typos, C-keyword collisions): inline comment beside the field explaining why the quirk stays | See `security_regs.h:21-22` for the `auteo` example: SDK uses misspelled `auteo` instead of `auto` (C++ keyword); renaming breaks SDK code interop. |

### What does NOT belong in `*_regs.h`

- **Operation-model commentary** — multi-paragraph blocks describing transfer invariants, state machines, edge cases. Belongs in `docs/hardware/<block>.md` via the `hardware-block-reference` skill. The block comment in `gdma_regs.h:57-80` is legacy from before that split; do not replicate.
- **Driver API surface** — function declarations, opcode enums beyond what the register encodes, status code typedefs. Those go in the driver's own header.
- **Build/integration notes, project history, TODOs.**

The header carries hardware layout. Nothing else.

## Path convention

| Where | Use when |
|---|---|
| `src/platform/drivers/hardware_<block>/<block>_regs.h` | **Canonical.** New files always go here. |
| `src/platform/drivers/hardware_<block>/include/hardware/<block>_regs.h` | **Anti-pattern.** Existing in `flash` and `sctrl` due to historical inclusion constraints from other drivers. Do NOT replicate for new headers. If you encounter it on a refactor explicitly tasked with re-pathing, fine; otherwise leave the existing two alone and don't propagate the layout. |

## Workflow — generation

1. Read the SDK fragment (file or pasted content).
2. **Resolve `#if CFG_SOC_NAME` gating** using `bk7252-sdk-reading`. Keep only branches active for `SOC_BK7221U`; discard inactive branches — do not carry them into the output.
3. **Identify the base address** and the **register offsets** (word indices `N` in `BASE + N * 4`). Build a flat list: `(offset, name, kind)` where `kind ∈ {bit-field register, data register, gap}`.
4. **Translate each bit-field register**:
    - Group together every `<REG>_<FIELD>_POSI` / `_MASK` pair AND every `(1<<N)` flag that belongs to this register.
    - Convert mask → bit-width via the mask table in `bk7252-sdk-reading`.
    - Lay out bit-fields in ascending `POSI` order inside a `union { uint32_t v; struct { ... }; }`.
    - Insert `reserved_<lo>_<hi>` for unallocated bit ranges so every union totals 32.
    - Add inline `// [bits]` + semantic comment per field (rule #3).
5. **Translate data registers** as plain `uint32_t name;`.
6. **Insert gap padding** between registers when offsets are non-contiguous, using rule #9.
7. **Apply naming**: typedef → `hw_<block>_t`, pointer macro → `hw_<block>`, base macro → `<BLOCK>_BASE_ADDR`. Field names follow SDK names where reasonable; rename only for C-keyword collisions (document the rename inline, per rule #11) or to drop pure SDK noise prefixes like `GDMA_X_`.
8. **Verification markers**: apply per rule #10 — sparingly, per-field, only when there's a real reason to doubt the behaviour. Default position is trust.
9. **Sanity checks** before delivering:
    - Every bit-field union sums to exactly 32.
    - Every register offset accounted for (registers + gap placeholders cover the full address range up to the last register).
    - File compiles in isolation — `register_defs.h` is present; any additional includes are standard headers only.
    - File path is `src/platform/drivers/hardware_<block>/<block>_regs.h` (NOT the nested anti-pattern path).
10. Final pointer macro line at file bottom: `#define hw_<block>  ((volatile hw_<block>_t *) <BLOCK>_BASE_ADDR)`.

## Workflow — refactor existing

Same target canon. Only run when **explicitly asked**.

Common legacy deltas to fix:

- Bit-range comments `[lo:hi]` → flip to `[hi:lo]` (uart_regs.h, efuse_regs.h).
- Struct-first union → `v`-first union (gdma_regs.h, security_regs.h, gpio_regs.h).
- Typedef `<block>_hw_t` / `*_reg_t` / `icu_<...>_reg_t` → `hw_<block>_t` (efuse_regs.h, intc_regs.h).
- Operation-model multi-paragraph comments in the header → relocate to `docs/hardware/<block>.md` and remove from header (gdma_regs.h block comments).
- Missing inline bit-comments → add per rule #3.

When refactoring, never break existing field names or change struct member offsets without a clear reason — call sites compile against them. Field renames need a sweep through callers.

## Real examples from this project

- `src/platform/drivers/hardware_gdma/gdma_regs.h` — closest existing file to canon (apart from the legacy operation-model block comment, which belongs in `docs/hardware/dma.md`).
- `src/platform/drivers/hardware_security/security_regs.h` — multi-engine block (AES, SHA, RSA). Demonstrates the `auteo` SDK-quirk comment pattern.
- `src/platform/drivers/hardware_random/random_regs.h` — minimal canonical form for a tiny block.
- `src/platform/drivers/common/include/register_defs.h` — the required base include; provides `hw_write_fields` for atomic multi-field stores and pulls `<stdint.h>` (orthogonal to this skill, but worth knowing).

## Final pass before delivering

Read the output as a future engineer who has never seen the block:

- Is every bit-field's width and position obvious from the code without consulting the SDK?
- Does the file declare exactly the hardware layout, nothing more?
- Are verification markers placed only where there's a real reason to doubt, not by reflex?
- Is the path `hardware_<block>/<block>_regs.h`, not the nested anti-pattern?

If any answer is wrong, fix it before delivering.
