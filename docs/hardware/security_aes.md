# BK7252 Hardware AES

The security block at `0x00806000` also contains an AES engine at offset `0`
(`SECURITY_AES_BASE = 0x00806000`). It computes a single-block ECB
transformation per `next` — the caller is responsible for any chaining mode
(CBC, CTR, etc.) and for padding.

## Register map

| Offset (words) | Name           | Purpose                                        |
| -------------- | -------------- | ---------------------------------------------- |
| `0x00..0x07`   | `key_0to7`     | Key material, big-endian words                 |
| `0x08`         | `control`      | `bit0 = INIT`, `bit1 = NEXT`, `bit2 = AUTEO`, `bit3 = INT_EN` |
| `0x09`         | `status`       | `bit0 = ready`, `bit1 = valid`, `bit2 = int_flag` |
| `0x0A`         | `config`       | `bit0 = encode`, `bits1..2 = mode`             |
| `0x0B`         | —              | reserved                                       |
| `0x0C..0x0F`   | `block_0to3`   | Input block, 16 B, big-endian words            |
| `0x10..0x13`   | `result_0to3`  | Output block, 16 B, big-endian words           |

### Mode

| Value | Algorithm | Key size | Key words used   |
| ----- | --------- | -------- | ---------------- |
| `0`   | AES-128   | 128 b    | `key_0to7[0..3]` |
| `1`   | AES-256   | 256 b    | `key_0to7[0..7]` |
| `2`   | AES-192   | 192 b    | `key_0to7[0..5]` |

Note the ordering — `AES256` sits at value `1`, `AES192` at value `2`.

### Direction

`config.encode = 1` — encryption. `config.encode = 0` — decryption. The
direction is consumed by `init` (the engine derives the appropriate key
schedule for that direction), so it must be set before `init` is written.

## Operation sequence

1. Set `config.encode` (direction) and `config.mode` (key size).
2. Write the key into `key_0to7` (only the words required by `mode` are
   consulted).
3. Write `control.init = 1` and poll `status.ready` until it reads `1`.
   At this point `status` is `0x01` (`ready = 1`, `valid = 0`) — the key
   schedule is loaded and retained until the next `init`.
4. For each 16-byte block, in either direction:
    - Write the block as big-endian words into `block_0to3[0..3]`
      (byte 0 of the plaintext/ciphertext is the MSB of `block_0to3[0]`).
    - Write `control.next = 1` and poll `status.ready` until it reads `1`.
      `status` becomes `0x03` (`ready = 1`, `valid = 1`).
    - Read the result from `result_0to3[0..3]`, big-endian (byte 0 of the
      output is the MSB of `result_0to3[0]`).

The key schedule survives across blocks — `init` is only needed when the
key, direction, or mode changes.

## Notes

- The hardware transforms on every `init`/`next` write. Writing both bits
  together (or any other unintended combination) runs two operations
  back-to-back and the intermediate state is lost — always write exactly
  one of `init` or `next` per `control` store, with the other bits clear.
- `AUTEO` — function not characterised; tests showed it does not cause
  auto-start on a subsequent `block_0to3` write. Leave it at `0`.
- `INT_EN` enables the AES completion interrupt. The IRQ routing through
  INTC has not been wired up; polling `status.ready` is sufficient for
  sequential use.
- Changing direction (encrypt↔decrypt) requires a new `init` because the
  decrypt schedule is the inverse of the encrypt schedule.
- Key, block, and result are all big-endian packed: the first byte of the
  logical 16/24/32-byte buffer lands in the most-significant byte of the
  first 32-bit word.