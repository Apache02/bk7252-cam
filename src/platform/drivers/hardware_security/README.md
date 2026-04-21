# BK7252 Hardware SHA

The security block at `0x00806000` contains a SHA engine at offset `0x40 * 4`
(`SECURITY_SHA_BASE = 0x00806100`). It computes the Merkle–Damgård compression
function — the caller is responsible for message padding (append `0x80`,
zero-pad, append big-endian bit-length).

## Register map

| Offset (words) | Name            | Purpose                                 |
| -------------- | --------------- | --------------------------------------- |
| `0x00..0x1F`   | `block_31to0`   | Input message block, big-endian words   |
| `0x20..0x2F`   | `digest_15to0`  | Current chaining state / final digest   |
| `0x30`         | `control`       | `bit0 = INIT`, `bit1 = NEXT`            |
| `0x31`         | `status`        | `bit0 = ready`, `bit1 = valid`          |
| `0x32`         | `config`        | `bits0..2 = mode`, `bit3 = enable`      |

### Mode

| Value | Algorithm    | Block size | State | Digest |
| ----- | ------------ | ---------- | ----- | ------ |
| `0`   | SHA-1        | 64 B       | 160 b | 160 b  |
| `2`   | SHA-224      | 64 B       | 256 b | 224 b  |
| `3`   | SHA-256      | 64 B       | 256 b | 256 b  |
| `4`   | SHA-512/224  | 128 B      | 512 b | 224 b  |
| `5`   | SHA-512/256  | 128 B      | 512 b | 256 b  |
| `6`   | SHA-384      | 128 B      | 512 b | 384 b  |
| `7`   | SHA-512      | 128 B      | 512 b | 512 b  |

## Operation sequence

1. Set `config.mode` and `config.enable = 1`.
2. For each padded message block:
   - Write the block as big-endian 32-bit words into the **tail** of
     `block_31to0`: word 0 goes to `block_31to0[32 - block_words]`,
     word `block_words - 1` goes to `block_31to0[31]`.
     (SHA-1/224/256 use indices 16..31; SHA-384/512 use 0..31.)
   - First block: write `control.init = 1` — loads the algorithm's IV and
     runs the compression.
   - Subsequent blocks: write `control.next = 1` — continues from the
     current chaining state.
   - Poll `status.ready` until it reads `1`.
3. Read the digest from the tail of `digest_15to0`:
   the state occupies the last `state_bits / 32` slots; truncated variants
   (SHA-224, SHA-384, SHA-512/t) use the first `digest_bits / 32` words of
   that region. Words are big-endian.

## Notes

- The hardware compresses on every `init`/`next` write — setting both bits
  in the same write produces two consecutive compressions.
- There is no length counter in hardware; software must track the total
  message length and emit the length field during padding.
- `status.valid` mirrors the digest-ready state after the final block;
  polling `ready` is sufficient for sequential use.
