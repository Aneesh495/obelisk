# Obelisk Intermediate Representation & Opcode Specification

## 1. Primary Instruction Set Architecture (ISA)

Obelisk lowers Brainfuck source text into an intermediate representation (IR) consisting of primary and synthetic instructions.

| Opcode | Mnemonic | Source Pattern | Formal Semantics |
| :--- | :--- | :--- | :--- |
| `0x00` | `IR_NOP` | N/A | No operation |
| `0x01` | `IR_ADD` | `+` / `-` | `tape[head] = (tape[head] + val) mod 256` |
| `0x02` | `IR_MOVE` | `>` / `<` | `head = head + val` |
| `0x03` | `IR_SET` | `[-]` / `[+]` | `tape[head] = val` |
| `0x04` | `IR_READ` | `,` | `tape[head] = read_fn()` |
| `0x05` | `IR_WRITE` | `.` | `write_fn(tape[head])` |
| `0x06` | `IR_JMP_IF_ZERO` | `[` | `if tape[head] == 0: pc = target` |
| `0x07` | `IR_JMP_IF_NZ` | `]` | `if tape[head] != 0: pc = target` |

---

## 2. Synthetic Super-Instructions (Tier 1 Optimizer)

During Tier 1 optimization, canonical Brainfuck idioms are synthesized into compound instructions to reduce instruction retirement count and eliminate branching:

### 2.1 Multiply-Add (`IR_ADD_MULT`)
* **Source Pattern**: `[ - > +++ < ]` or `[ > +++ < - ]`
* **Formal Semantics**:
  ```c
  tape[head + offset] = (tape[head + offset] + tape[head] * factor) mod 256;
  tape[head] = 0;
  ```
* **Performance Impact**: Replaces `O(N)` loop iterations with an `O(1)` arithmetic evaluation.

### 2.2 Forward Zero Scan (`IR_SCAN_FWD`)
* **Source Pattern**: `[ > ]`
* **Formal Semantics**:
  ```c
  while (tape[head] != 0) head += stride;
  ```
* **Acceleration**: On systems supporting vector operations, lowered to libc `memchr` or SIMD scanning, searching up to 32 bytes per cycle.

### 2.3 Backward Zero Scan (`IR_SCAN_BCK`)
* **Source Pattern**: `[ < ]`
* **Formal Semantics**:
  ```c
  while (tape[head] != 0 && head > 0) head -= stride;
  ```
