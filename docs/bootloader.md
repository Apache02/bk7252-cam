# Vendor Bootloader Reference (`bootloader_a9_v720.bin`)

Documents the vendor bootloader that ships in the `bootloader` flash partition
(`0x000000–0x00FFFF`). This bootloader is being replaced — see
`docs/superpowers/specs/2026-06-07-bootloader-design.md`.

---

## Exception vector dispatch

The bootloader's ARM exception vector table starts at `0x00000000` and
dispatches through a two-level indirection before reaching the hook table in RAM
(see `docs/memory_map.md` — *Hook table*).

### Level 1 — ARM vector table (`0x00000000–0x0000001C`)

```
0x00000000  EA0000C5  b <reset>           ; branches to reset handler at 0x000003CC
0x00000004  E59FF014  ldr pc, [pc, #0x14] ; UNDEFINED  → [0x00000020]
0x00000008  E59FF014  ldr pc, [pc, #0x14] ; SWI        → [0x00000024]
0x0000000C  E59FF014  ldr pc, [pc, #0x14] ; PABORT     → [0x00000028]
0x00000010  E59FF014  ldr pc, [pc, #0x14] ; DABORT     → [0x0000002C]
0x00000014  E59FF014  ldr pc, [pc, #0x14] ; RESERVED   → [0x00000030]
0x00000018  E59FF014  ldr pc, [pc, #0x14] ; IRQ        → [0x00000034]
0x0000001C  E59FF014  ldr pc, [pc, #0x14] ; FIQ        → [0x00000038]
```

### Level 2 — dispatch address table (`0x00000020–0x0000003C`)

| Address      | Value        | Points to                       |
|--------------|--------------|---------------------------------|
| `0x00000020` | `0x00000608` | Bootloader UNDEFINED handler    |
| `0x00000024` | `0x0000059C` | Bootloader SWI handler          |
| `0x00000028` | `0x00000618` | Bootloader PABORT handler       |
| `0x0000002C` | `0x00000628` | Bootloader DABORT handler       |
| `0x00000030` | `0x00000638` | Bootloader RESERVED handler     |
| `0x00000034` | `0x000005AC` | Bootloader IRQ handler          |
| `0x00000038` | `0x000005BC` | Bootloader FIQ handler          |
| `0x0000003C` | `0xDEADBEEF` | Sentinel / padding              |

### Level 3 — bootloader handlers → hook table dispatch

Each handler body follows the same pattern (verified by disassembly):

```asm
push  {r0, r1}
ldr   r1, =<hook_slot_address>   ; from literal pool
ldr   r1, [r1]                   ; read firmware handler from hook table
bx    r1                          ; tail-call dispatch
```

Hook slot addresses read by each handler:

| Handler   | Hook slot                                                                     |
|-----------|-------------------------------------------------------------------------------|
| IRQ       | `0x00400000` (`hook_IRQ`) — loaded via `mov r1, #0x400000` (immediate)       |
| FIQ       | `0x00400004` (`hook_FIQ`)                                                     |
| UNDEFINED | `0x0040000C` (`hook_undefined`) — confirmed routed to firmware handler        |
| PABORT    | `0x00400010` (`hook_pabort`)                                                  |
| DABORT    | `0x00400014` (`hook_dabort`) — reads correct slot; routing to IRAM handler unconfirmed |
| RESERVED  | `0x00400018` (`hook_reserved`)                                                |
