---
name: hardware-block-reference
description: Write hardware-block reference documentation for chips where the vendor datasheet is missing, incomplete, or wrong - the kind of document the vendor should have written but didn't. Use whenever the user wants to document a peripheral, security block, DMA controller, sensor interface, or any other on-chip hardware unit. Triggers on phrases like "write documentation for this hardware block", "document the registers", "make a hardware reference", "describe how this peripheral works", "reverse-engineered documentation", "turn my notes into a hardware doc", or when the user shares register-level details about a chip and asks for them to be written up. Use this even if the user just says "write a README for the X block" - the right output is a hardware reference, not a project README.
---

# Hardware Block Reference

This skill produces concise reference documentation for on-chip hardware units when no usable vendor documentation exists. The audience is a future engineer (often the same person, six months later) who needs to write firmware against the block and has nothing else to go by. Treat the output as if the silicon vendor had written it — terse, structured, register-first, free of project history.

## What this skill is for, and what it isn't

The output is **hardware reference**, not API documentation, not a project README, not a tutorial. The reader wants to know what the silicon does when you write specific bits into specific registers. They do not want:

- API surface of a driver written on top of the block (`my_driver_init()` calls)
- Roadmap, TODO, "what's next" sections
- Project history ("we figured this out by running tests with...")
- Links to source files in the project (`gdma.c`, `gdma.h`)
- Build instructions, prerequisites, "how to integrate" sections

If the user's source material mixes these concerns, **separate them out** in your draft. The hardware-reference document gets register behaviour; everything else belongs in a separate project-README that lives next to the driver code.

## Required sections

In this order, omitting any that genuinely don't apply:

1. **Title and one-paragraph orientation** — what the block is, where it sits in the memory map, what it computes or moves, what's connected to it.
2. **Register map** — tables of offsets, names, purposes. One table per logical group (per-channel block, global block, per-bank, etc.). Bit-field breakdowns get their own sub-table when a register has more than three meaningful fields.
3. **Operation model** — the conceptual story of what the block does between writes. This is the most valuable section when the vendor docs are missing, because regs without a behavioural model are noise. For simple blocks (a UART, a single-shot crypto compressor) the model may be implicit in the operation sequence; for blocks with internal state (DMA accumulators, pipelined hashers) it deserves its own named subsection.
4. **Operation sequence** — numbered steps for the typical usage. Each step is one or two register writes plus a poll. The reader should be able to translate this directly into firmware.
5. **Interrupts** — if the block raises IRQs, describe the line, the status bits, the ack mechanism. Skip this section entirely if the block has no interrupts.
6. **Notes** — bulleted list of behavioural details that didn't fit the structured sections: sticky bits, side effects, untested fields, gotchas, mode-switching constraints. This is where "if you write both bits in one store, two operations run back-to-back" lives.

## Tone

Imperative and declarative. Drop hedge words. Compare:

- **Bad:** "It seems that the controller will probably perform `transfer_length + 1` writes."
- **Good:** "The controller performs `transfer_length + 1` dst writes."

When something is uncharacterised, say so plainly: "Function not characterised. Leave at 0." Don't pad with "we didn't have time to test this but probably..." — the reader gains nothing from speculation marked as such.

When something is non-obvious and would mislead an experienced reader, **call it out explicitly with bold**, the same way the vendor docs would have if they were honest. Example: "**The SDK names `8BIT/16BIT/32BIT` describe peripheral-side lane width, not memory throughput.**" The bold is earned by the gotcha, not by emphasis-for-emphasis-sake.

Do not use first person ("we found", "I tested"). Do not address the reader directly except in the imperative ("Write 1 to clear", "Poll `status.ready` until..."). Do not use marketing voice ("powerful", "flexible", "easy-to-use") — this is reference, not a product page.

## Register map conventions

Use Markdown tables. Columns vary by what's being described:

**For register listings:**
```markdown
| Offset (words) | Name        | Purpose                       |
| -------------- | ----------- | ----------------------------- |
| `0x00..0x07`   | `key_0to7`  | Key material, big-endian words |
| `0x08`         | `control`   | `bit0 = INIT`, `bit1 = NEXT`  |
```

**For bit fields in a single register:**
```markdown
| Bits      | Name             | Purpose                              |
| --------- | ---------------- | ------------------------------------ |
| `[0]`     | `enable`         | 1 = channel runs; hw clears at completion |
| `[5:4]`   | `src_data_width` | `0=8b`, `1=16b`, `2=32b`             |
```

**For enumerated values of a multi-bit field (mode, request lines, etc.):**
```markdown
| Value | Algorithm   | Block size | State | Digest |
| ----- | ----------- | ---------- | ----- | ------ |
| `0`   | SHA-1       | 64 B       | 160 b | 160 b  |
```

Specify offsets in the unit the silicon actually uses (words or bytes). State this in the section header — "Offset (words)" vs "Offset (bytes)". If both unit conventions appear in the user's notes, pick one and stick to it.

Address ranges as `0x00..0x07` (inclusive). Bit ranges as `[5:4]` (MSB:LSB), single bits as `[3]`. Numeric constants always in hex with `` `0x...` `` backticks; bit values without backticks when in prose ("set bit to 1") and with backticks in tables for visual alignment.

## Operation model — the hard part

When the block has internal state that survives between register writes (accumulators, key schedules, chaining state, FIFOs), this section is where the reference earns its keep. Conventions:

- Lead with the **invariant**: what the model promises regardless of width / mode / sequence. ("A channel performs exactly `transfer_length + 1` dst writes.")
- Explain the **state**: what the block remembers between operations. Diagram if it helps, but a short paragraph usually suffices.
- Cover the **edge cases** the model explains: off-by-ones, tail artefacts, what happens at boundaries (single-element transfers, mode changes, key swaps).
- Resist the urge to include test data. The model is more valuable than the evidence. If the reader wants to verify, they can run the tests themselves; the document records what's true, not how it was discovered.

For blocks without persistent state (a single-block AES ECB transform, a one-shot CRC), the operation sequence section absorbs this content and no separate model section is needed.

## Operation sequence

Numbered steps. Each step is one logical action: a register write, a poll, a read. Group sub-bullets only when one logical step requires multiple writes (e.g. loading a multi-word key, programming all four endpoint addresses). Example shape:

```markdown
1. Set `config.mode` and `config.enable = 1`.
2. For each padded message block:
    - Write the block as big-endian words into `block_31to0[16..31]`.
    - First block: write `control.init = 1`.
    - Subsequent blocks: write `control.next = 1`.
    - Poll `status.ready` until it reads `1`.
3. Read the digest from `digest_15to0[...]`, big-endian.
```

If two distinct flows exist (encrypt vs decrypt, single-shot vs streaming), give each its own numbered list or clearly bracket them within one list.

## Notes section

Catch-all for behavioural details. Each note one to three sentences. Common categories:

- **Self-clearing or sticky bits** — "`enable` is self-clearing on completion. `fin_count` is sticky and never auto-clears."
- **Side effects** — "Writing `init` discards the current key schedule."
- **Forbidden combinations** — "Writing both `init` and `next` in one store runs two operations back-to-back; the intermediate state is lost."
- **Defaults that matter** — "`prio_mode.fixed_priority = 0` selects round-robin and is the most useful default."
- **Uncharacterised fields** — "`AUTEO` — function not characterised. Tests showed it does not cause auto-start on a subsequent block write. Leave at `0`."
- **Telemetry vs control** — distinguish registers used for debug from those used in normal operation.

Notes are not the place for usage advice that belongs in operation sequence, nor for caveats that belong inline next to the register they affect. If a note keeps wanting to grow into a paragraph, it probably wants to be promoted to a subsection of the operation model.

## Things to leave out

These tend to creep in when the user's source material is mixed-purpose. Strip them:

- Driver API descriptions (`gdma_reserve_channel()` etc.)
- Error code enums from the driver
- Workarounds in driver code ("there's a TODO in `gdma.c`...")
- Roadmaps ("next we will add loop mode support")
- "How we figured this out" narratives
- File listings of the project
- Test methodology, RSA-bank tricks, instrumentation details
- Build, install, integration instructions
- Acknowledgements, status badges, version history

If the user explicitly wants any of this, suggest a separate project-README and keep the hardware-reference clean.

## Working with the user

The user will typically arrive with one of three starting points:

1. **Raw notes / shell transcripts / register dumps** — extract structure. Ask which register block this is, where it sits in the memory map, and what the block does at a high level. Build the register map first because everything else hangs off it.
2. **A mixed README that has both hardware notes and project context** — separate the two. Produce the hardware-reference from the hardware parts; offer to write or update a separate project-README from the rest.
3. **An existing hardware-reference they want to expand or fix** — preserve structure, edit in place. Check whether new information changes the operation model — if so, the model section needs revision, not just an addendum in notes.

Ask one or two targeted questions if anything in the source is ambiguous (especially: is this offset in words or bytes? what does the value `N` in `transfer_length` mean — bytes, writes, units?). Don't ask exhaustive interview questions before drafting — produce a first draft and let the user point at what's wrong. Hardware documentation iterates better than it interviews.

## Output file

Name the file after the block, not "README.md". Place it in a `docs/` or `docs/hardware/` directory if that pattern exists in the project. Match the casing and word-separator style already used by sibling docs in the target directory — pick the convention from what's there (`gdma.md` / `GDMA.md` / `gdma_controller.md`) rather than imposing one. When a parent block contains independent sub-engines (e.g. a security block with both SHA and AES), prefer a grouping prefix per sub-engine (`security_aes.md`, `security_sha.md`) over a single combined file; keep each engine as one Markdown file unless the sub-units are genuinely separable.

## Length

A typical block lands at 150–400 lines. Below ~80 lines the document is probably underspecified — likely the operation model section is missing or skinny. Above ~600 lines the document is probably trying to cover multiple blocks or has crept into driver-API territory; split or trim.

## Final pass

Before delivering, read the draft as if you've never seen the block before. The questions to ask:

- Could I write firmware against this block using only this document?
- Is there anywhere I'd guess wrong because the document doesn't say?
- Is there anything here that isn't about the silicon?

The first two should be answerable "yes / no". The third should be "no". If any answer is wrong, fix that section before delivering.
