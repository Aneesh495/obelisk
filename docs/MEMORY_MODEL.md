# Obelisk Memory Model: Turing Tape Virtual Segmentation

## 1. Abstract Memory Specification

The Obelisk execution environment models memory as a continuous 1-D array of 8-bit unsigned cells:
```
Tape = [Cell_0, Cell_1, Cell_2, ..., Cell_{N-1}]
where Cell_i in {0, 1, ..., 255}
```
All arithmetic on individual cells adheres to standard modulo-256 wrapping rules. The tape pointer index `P` is constrained to the closed interval `[0, N-1]`. Any attempt to decrement `P` below zero or advance `P` beyond `N-1` triggers an immediate `OBELISK_ERR_OUT_OF_BOUNDS` termination status, preventing undefined memory behavior.

---

## 2. Linear Virtual Segmentation

The 1-D tape is segmented into deterministic functional regions:

```
+---------------+-------------------+------------------+------------------+---------------------+
| 0x0000-0x00FF |   0x0100-0x03FF   |  0x0400-0x1FFF   |  0x2000-0x3FFF   |    0x4000-0xFFFF    |
| Scratch / Reg | I/O Ring Buffers  |  Call Stack & BP | WAL & Undo Log   | B-Tree Index & Data |
+---------------+-------------------+------------------+------------------+---------------------+
```

### 2.1 Scratch Space and Virtual Register Window (0x0000 - 0x00FF)
* **Cells 0 - 15**: General-purpose scratch registers (`t0` through `t7`), used for intermediate arithmetic, multiplication accumulators, and division remainder cells.
* **Cells 16 - 31**: Virtual register file (`r0` through `r15`), mapped to compiler variables and active operation operands.
* **Cells 32 - 255**: Static lookup tables and constants (e.g. ASCII delimiter masks, decimal digit lookup tables).

### 2.2 I/O Staging Buffers (0x0100 - 0x03FF)
* **Cells 256 - 511**: Inbound request staging buffer. Streaming socket input accumulates in this segment prior to wire protocol parsing.
* **Cells 512 - 1023**: Outbound response serialization buffer. Formatted RESP payloads assemble here before streaming to output file descriptors.

### 2.3 Call Stack & Activation Frames (0x0400 - 0x1FFF)
* **Stack Pointer (SP)** and **Base Pointer (BP)** track activation frames.
* Each simulated stack frame preserves caller register state, return branch markers, and local variables.

### 2.4 Transaction Write-Ahead Log (0x2000 - 0x3FFF)
* Tracks transactional mutations.
* Each entry stores key length, key bytes, previous value length, previous value bytes, and existence flag.

### 2.5 B-Tree Ordered Index & Storage Heap (0x4000 - 0xFFFF)
* Fixed-size index partitions storing SipHash-2-4 key hashes and payload pointers.
* Record payloads stored as length-prefixed byte sequences.
