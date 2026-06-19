# SOC Register Header Conventions

MMIO register headers for the BK7252 (SOC `bk7221u`) live at:

```
src/platform/soc/bk7221u/include/soc/<block>.h
```

Included as `#include "soc/<block>.h"`. All drivers receive the include path transitively via `platform_soc` (linked by `simple_drivers_library()`).

---

## File skeleton

```c
#pragma once

#include "platform/soc.h"      // pulls <stdint.h>; exposes hw_write_fields


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

    uint32_t data;                         // plain data register (no internal bit structure)
} hw_<block>_t;

#define hw_<block>      ((volatile hw_<block>_t *) <BLOCK>_BASE_ADDR)
```

---

## 12 conventions

### 1. Union layout: `v` first, struct second

```c
union {
    uint32_t v;         // full 32-bit word alias — must be FIRST
    struct {
        uint32_t field_a: 4;    // [3:0]
        uint32_t field_b: 28;   // [31:4]
    };
} reg_name;
```

`v` first makes the atomic-write alias discoverable on first scan and matches `hw_write_fields()`.

### 2. Bit-range comments: `[hi:lo]` (MSB:LSB)

Single-bit field: `// [N]`  
Multi-bit field: `// [hi:lo]`

```c
uint32_t enable: 1;        // [0]
uint32_t mode: 2;          // [2:1]
uint32_t length: 16;       // [31:16]
```

Matches datasheet and `docs/hardware/*.md` convention. Legacy files using `[lo:hi]` are wrong; fix on refactor.

### 3. Every bit-field gets an inline `// [bits]` comment

Single-bit fields too — placement is cheap; future readers benefit.

```c
uint32_t repeat_mode: 1;   // [3]  0: single-shot, 1: loop using *_loop_*_addr (NOT TESTED)
uint32_t reserved_4_7: 4;  // [7:4]
```

Add a semantic note after the placement comment when the name alone is not self-evident.

### 4. Reserved bit-field naming

| Pattern | Use when |
|---------|----------|
| `reserved_<lo>_<hi>: N;` | reserved range of N bits (bits lo..hi) |
| `reserved_<n>: 1;` | single reserved bit |

All bit-fields in a union **must sum to exactly 32**.

```c
uint32_t enable: 1;              // [0]
uint32_t reserved_1_3: 3;        // [3:1]
uint32_t length: 16;             // [19:4]
uint32_t reserved_20_31: 12;     // [31:20]
```

### 5. Required include

`#include "platform/soc.h"` is required in every register header. It provides `hw_write_fields()` and pulls `<stdint.h>`. Additional standard headers are allowed when needed (e.g. `<assert.h>`).

### 6. Typedef name

| Pattern | Use for |
|---------|---------|
| `hw_<block>_t` | single memory-mapped block |
| `hw_<block>_<sub>_t` | sub-engine within a multi-engine block |

Examples: `hw_gdma_t`, `hw_gdma_channel_t`, `hw_security_aes_t`.

Legacy suffix `_reg_t` (e.g. `hw_security_aes_reg_t`) is a known legacy deviation — fix on refactor.

### 7. Base address macro

| Pattern | Use when |
|---------|----------|
| `#define <BLOCK>_BASE_ADDR (0x...)` | standalone block |
| `<PARENT>_BASE + N * 4` | block sits inside a parent's address space |

```c
#define GDMA_BASE_ADDR   (0x00809000)
#define EFUSE_BASE_ADDR  (SCTRL_BASE + 55 * 4)   // inside SCTRL
```

### 8. Pointer macro

```c
#define hw_<block>    ((volatile hw_<block>_t *) <BLOCK>_BASE_ADDR)
```

For multi-instance blocks: `hw_uart1`, `hw_uart2`, etc.  
The macro goes at the **bottom** of the file, after the typedef.

### 9. Gap padding between non-contiguous registers

```c
uint32_t reserved_0x36;              // single-word gap at word offset 0x36
uint32_t reserved_0x39_0x3F[7];      // multi-word gap, word offsets 0x39..0x3F
```

Rules:
- Names use hex word offsets, e.g. `reserved_0x<lo>_0x<hi>`.
- No `_word_` infix.
- Single word: `reserved_0x<offset>;` (no array).
- Multiple words: `reserved_0x<lo>_0x<hi>[N];`.

### 10. Verification markers

Use sparingly and per-field — **default position is to trust the SDK**.

| Marker | Meaning |
|--------|---------|
| `(NOT TESTED)` | Semantics from SDK; not exercised in this project |
| `(NOT INVESTIGATED)` | Never examined |
| `(from SDK)` | Copied verbatim without independent validation |
| `(NOT FUNCTIONAL IN SILICON)` | Silicon does not honour the SDK description |

Apply to specific fields with a real reason to doubt, not to entire blocks.

### 11. SDK quirks

Document inline any deviation from the SDK identifier that was forced by a language constraint:

```c
uint32_t auteo: 1;   // [0]  SDK spells it "auteo" (not "auto"); keeping SDK name avoids
                     //       interop issues; "auto" is a C++ keyword
```

### 12. Blank-line spacing between top-level blocks

Two blank lines separate each of the three top-level blocks from the next:

```c
#pragma once

#include "platform/soc.h"


#define <BLOCK>_BASE_ADDR    (0x00......)


typedef volatile struct {
    ...
} hw_<block>_t;

#define hw_<block>      ((volatile hw_<block>_t *) <BLOCK>_BASE_ADDR)
```

- After the last `#include` line → 2 blank lines → first `#define`
- After the last top `#define` line → 2 blank lines → `typedef`
- One blank line between the closing `} hw_<block>_t;` and the pointer macro is sufficient.

---

## What does NOT belong in a register header

- **Operation-model commentary** — state machines, transfer invariants, edge-case sequences. Belongs in `docs/hardware/<block>.md`.
- **Driver API surface** — function declarations, opaque status typedefs, opcode enums beyond what the register directly encodes.
- **Build notes, TODOs, project history.**

A register header carries hardware layout. Nothing else.

---

## Reference files

| File | What it demonstrates |
|------|----------------------|
| `soc/gdma.h` | Canonical form; multi-register channel struct; gap padding |
| `soc/security.h` | Multi-engine block (AES/SHA/RSA); `auteo` SDK-quirk pattern |
| `soc/random.h` | Minimal single-register canonical form |
| `soc/intc.h` | Non-MMIO `typedef struct` (software only, not volatile); pointer macros for registers at explicit offsets |
| `platform/soc.h` | Required base include; `hw_write_fields` definition |

---

## Generating a new header

1. Read the SDK fragment (`beken378/driver/<block>/<block>.h` or `<block>_reg.h`).
2. Resolve `#if CFG_SOC_NAME` gating — keep only `SOC_BK7221U` branches.
3. Map each `_POSI` / `_MASK` pair to a bit-field.
4. Convert mask to width: `0x1`→1, `0x3`→2, `0x7`→3, `0xF`→4, `0x1F`→5, `0xFF`→8, `0x3FF`→10, `0xFFF`→12, `0xFFFF`→16.
5. Fill unallocated ranges with `reserved_<lo>_<hi>` so every union sums to 32.
6. Pad non-contiguous offsets with `uint32_t reserved_0x<lo>_0x<hi>[N];`.
7. Apply all 11 conventions above.
8. Sanity-check: every union sums to 32; every offset accounted for; file path is `soc/<block>.h`.
