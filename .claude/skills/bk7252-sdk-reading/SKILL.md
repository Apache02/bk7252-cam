---
name: bk7252-sdk-reading
description: Use when reading or navigating Beken SDK source code for the BK7252 project — resolving CFG_SOC_NAME preprocessor guards, identifying which SDK branches apply to this chip, finding register definitions, or understanding SDK encoding conventions. Triggers on: pasted SDK fragments with #if CFG_SOC_NAME guards, questions about which chip variant a code block applies to, "which branch should I use", "does this apply to our chip", "where is X defined in the SDK", or any task that requires interpreting beken_freertos_sdk source before porting or translating it.
---

# BK7252 SDK Reading Guide

Reference for navigating and interpreting `beken_freertos_sdk` source when working on the BK7252 project. Always resolve chip identity and branch gating before porting any SDK code.

**REQUIRED SUB-SKILL for register output:** Use `bk7252-register-headers` when the goal is to produce a canonical `soc/<block>.h`.

## Chip identity

This project targets the **BK7252 / BK7221U** chip:

```c
#define SOC_BK7231   1   // different family — ignore all its branches
#define SOC_BK7231U  2   // different family — ignore all its branches
#define SOC_BK7221U  3   // ← THIS PROJECT (BK7252)
#define SOC_BK7231N  5   // different family — ignore all its branches
#define SOC_BK7252N  8   // DIFFERENT CHIP — despite "BK7252" in the name,
                         //   critical peripheral and register differences;
                         //   treat its branches as foreign hardware, same as SOC_BK7231

#define CFG_SOC_NAME  SOC_BK7221U   // = 3
```

Reference config: `beken_freertos_sdk/beken378/app/config/sys_config_bk7251.h`

## Branch resolution

A branch is **active** iff the expression evaluates true with `CFG_SOC_NAME = SOC_BK7221U` (3).

| SDK gate | Active for BK7252? |
|---|---|
| `#if CFG_SOC_NAME == SOC_BK7221U` | ✅ yes |
| `#if CFG_SOC_NAME == SOC_BK7252N` | ❌ no — different chip |
| `#if CFG_SOC_NAME == SOC_BK7231` | ❌ no |
| `#if CFG_SOC_NAME == SOC_BK7231N` | ❌ no |
| `#if CFG_SOC_NAME != SOC_BK7231` | ✅ yes |
| `#if CFG_SOC_NAME != SOC_BK7221U` | ❌ no |
| `#if CFG_SOC_NAME == SOC_BK7221U \|\| CFG_SOC_NAME == SOC_BK7252N` | ✅ yes (BK7221U arm only) |
| `#if CFG_SOC_NAME == SOC_BK7231 \|\| CFG_SOC_NAME == SOC_BK7231U` | ❌ no |

Resolve `#elif` / `#else` in sequence — first matching arm wins; later arms are inactive even if also true. Apply recursively to nested `#if`s.

**Discard inactive branches entirely** when porting. Output must not carry forward `#ifdef`s — it is BK7252-specific.

## SDK source layout

```
beken_freertos_sdk/beken378/driver/<block>/
  <block>.h        # register constants (_POSI, _MASK, base address, flags)
  <block>_reg.h    # sometimes split here instead
  <block>.c        # driver implementation — read to validate register semantics
beken378/app/config/sys_config_bk7251.h   # CFG_SOC_NAME and all feature flags
```

## SDK register encoding patterns

| SDK pattern | Meaning |
|---|---|
| `#define <BLOCK>_BASE  (0x00......)` | Block base address |
| `#define <REG>  (<BLOCK>_BASE + N * 4)` | Register at word offset N |
| `#define <FIELD>  (0x01UL << N)` or `(1 << N)` | Single-bit flag at bit N |
| `#define <FIELD>_POSI  (N)` | Multi-bit field, start bit |
| `#define <FIELD>_MASK  (M)` | Multi-bit field, width-as-mask |
| `#define <VALUE_NAME>  (0xN)` | Numeric enum value for a multi-bit field |
| `#define <REG>` with no further constants | Plain data register (no bit-fields) |

**Mask-to-width** (masks are always low-bit-contiguous in Beken SDK):

| MASK | Width | MASK | Width |
|---|---|---|---|
| `0x1` | 1 | `0xff` | 8 |
| `0x3` | 2 | `0xfff` | 12 |
| `0x7` | 3 | `0xffff` | 16 |
| `0xf` | 4 | `0x1ffff` | 17 |
| `0x1f` | 5 | `0xfffff` | 20 |
| `0x3f` | 6 | `0xffffff` | 24 |
| `0x7f` | 7 | `0xffffffff` | 32 |

If a mask is NOT a contiguous low-bit run (e.g. `0x6`, `0x14`) — SDK bug or non-contiguous field. Read the surrounding `.c` before translating.

## SDK reliability

**Default: trust the SDK.** It has been compiled and run on this exact silicon.

Suspect specifically when:
- The block has no usage examples in any SDK `.c`
- The field is debug/telemetry rather than control
- The user says they haven't exercised it on hardware

Known unreliable areas in this project:
- **Security RSA engine**: SDK ships only constants, no `.c` that exercises them. `status`/`config` bit layouts were wrong on initial inspection; base address and offsets were correct.
- **Timer read-counter path** (`read_ctl` / `read_value`): SDK describes a mechanism to read the current counter; silicon does not honour it — readback returns garbage. Mark `(NOT FUNCTIONAL IN SILICON)`.
