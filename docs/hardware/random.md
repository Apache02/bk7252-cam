# BK7252 True random number generator

Random source at `0x00802480`. Two 32-bit registers: an enable bit and a data word.
Consecutive reads of the data word are not independent: how they relate depends on the
time between the reads.

---

## Register map

Offset in bytes from `0x00802480`.

| Offset | Name   | Purpose                                              |
| ------ | ------ | ---------------------------------------------------- |
| `0x00` | `ctrl` | `bit0 = enable`; reads back what was written         |
| `0x04` | `data` | Random output word while enabled; latch while disabled |

### `ctrl`

| Bits     | Name     | Purpose                                      |
| -------- | -------- | -------------------------------------------- |
| `[0]`    | `enable` | 1 = generator running; keep set while reading |
| `[31:1]` | reserved | Read as `0`. No ready or status bit exists   |

---

## Operation model

Each read of `data` returns one 32-bit word. While `enable = 0`, `data` is a plain 32-bit
latch: a written value reads back unchanged. The written value has no effect on the words
produced after the next enable.

### Dependency between reads close in time

**Reads issued close together carry only a few independent bits each.** Four consecutive
reads span 128 bits; the GF(2) rank of those bits, taken over 500 groups, depends on the
number of `nop` instructions between the loads:

| `nop`s between loads | Independent bits of 128 |
| -------------------- | ----------------------- |
| `0`                  | `39..42`                |
| `5`                  | `51..55`                |
| `10`                 | `60..62`                |
| `20`                 | `74..86`                |

The number of independent bits grows with the time between reads. Single reads separated by
a delay from the hardware microsecond counter, from `0.5 µs` to `512 µs` (16 values), showed
no rank deficiency at any delay.

### Spacing sensitivity of consecutive words

For words built from four back-to-back reads (last read kept), the fraction of 32×32 GF(2)
matrices of 32 consecutive words with rank `<= 30` is `0.08..0.25` at most spacings (random
data: `0.134`) and `0.50..0.90` at isolated spacings. The affected spacings depend on the
instruction layout of the reading code: `5` and `10` `nop`s in one image, `2`, `7` and `16`
in another. At an affected spacing, every one of the four reads of a word fails the same
test on its own.

In an affected image, the last of four back-to-back reads failed NIST STS `Rank` on 0 of 33
sequences of 1 Mbit and passed every other STS test.

### Overlap between consecutive reads

Whether consecutive reads share bits is not characterised. Measurements gave a shift of 8
bits per read (reads right after reset), a shift of 16 bits per read on all 4095 consecutive
pairs of one 4096-read measurement (`next[31:16] == prev[15:0]`), and no overlap in twelve
later measurements of 256 reads each. Do not rely on overlap or on its absence.

### Warm-up

Reads right after the first enable following a chip reset returned a fixed sequence in one
measurement:

```
00 02 46 94 e3 0b 4b ...
```

i.e. the words `0x00024694`, `0x024694e3`, `0x4694e30b`, `0x94e30b4b`. Another measurement,
with the reads starting immediately after the enable, returned different words
(`0x5a59180b`, `0x602cfa2b`, `0xe8afa2b9`). With a delay of 16 µs or more between enable and
the first read, the first word differs on every reset. The threshold lies between 0 and
16 µs; it was not narrowed further. 32 µs gives a factor of two over the smallest delay that
worked.

No register reports when the generator is ready, so the delay cannot be replaced by
polling.

---

## Operation sequence

1. Write `ctrl.enable = 1`.
2. Wait at least 32 µs.
3. Read `data`. Treat each read as one sample and separate samples by at least 1 µs. Do not
   use back-to-back reads as independent words.
4. Write `ctrl.enable = 0` to stop the generator when no longer needed.

---

## Test results

| Test | Input | Result |
| ---- | ----- | ------ |
| GF(2) rank of 4 consecutive reads (128 bits), 500 groups | raw reads, `0..20` `nop`s between loads | Rank `39..42` at `0` `nop`s, rising to `74..86` at `20`. Close reads are linearly dependent |
| GF(2) rank of single reads | raw reads, `0.5..512 µs` apart | No rank deficiency at any spacing |
| Frequency, runs, byte histogram | 128 KiB of words from four back-to-back reads | No deviation |
| NIST STS, all tests | 33 sequences of 1 Mbit, last of four back-to-back reads | `Rank` failed on 0 of 33 sequences; every other test passed. Result depends on the instruction layout of the reading code |
| Rank of 32×32 matrices | XOR of four reads; XOR with shifts of four reads | Fraction with rank `<= 30` stays at `0.22` and `0.76..0.96` (random data: `0.134`). XOR and shifts do not remove the dependency |
| NIST STS, all tests | 33 sequences of 1 Mbit, each word = 16 rounds of `w = w * 1664525 + 1013904223` XOR a new read | All tests passed except the open flags below. Rank over 32768 matrices: `0.1347` (expected `0.1336`) |
| Rank of 32×32 matrices | 512 matrices, same conditioning | `78` with rank `<= 30` (expected about `68`) |
| Repeat of consecutive words | 1000 words, same conditioning | No equal neighbours |

**Open flags in the last STS run:** `Rank` uniformity `p = 0.0049` (borderline), and
`RandomExcursionsVariant` in 2 of 18 rows at `p < 1e-4`, with only 18 sequences applicable
to that test. Settling these needs at least 100 sequences (about 13 MB).

**Conclusion:** raw reads close in time are linearly dependent and fail the rank test.
Conditioning with a multiply-add chain (carries break the linear dependency) passed the
statistical suite. **The amount of entropy in the source is not measured.** STS checks
statistics only; an entropy estimate needs NIST SP 800-90B. The linear-state model of the
source is a hypothesis, not proven. Do not use raw or conditioned output directly as key
material; hash it first.

---

## Notes

- **Raw reads are not suitable as key material without conditioning.** Combining reads with
  XOR and shifts keeps the dependency: XOR of four reads has the same rank-deficient fraction
  (`0.22`) as a single read, and XOR with right shifts of four reads gives `0.76..0.96`
  across spacings.
- Frequency, runs and byte-histogram tests on words of four back-to-back reads showed no
  deviation over 128 KiB.
- Writing `data` while enabled is not characterised.
- Whether writing `enable = 1` again while already enabled restarts the warm-up is not
  characterised.
