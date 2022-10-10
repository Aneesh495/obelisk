# ADR-0001: Turing Tape Linear Memory Segmentation

## Status
Accepted

## Context
Brainfuck natively provides only an unbounded or fixed 1-D array of 8-bit cells with a single movable pointer. To construct an ACID transactional database engine with B-tree indexing and write-ahead logging, structured data layouts are mandatory.

## Decision
We segment the linear tape into deterministic virtual regions:
* 0x0000 - 0x00FF: Scratch space, virtual register window (`r0` through `r15`), and arithmetic temporary cells.
* 0x0100 - 0x03FF: Inbound and outbound I/O ring buffers.
* 0x0400 - 0x1FFF: Simulated call stack and base pointer activation records.
* 0x2000 - 0x3FFF: Transaction write-ahead log (WAL) and undo journal.
* 0x4000 - 0xFFFF: B-tree ordered index nodes and record payload storage.

## Consequences
* **Positive**: Guarantees deterministic memory layout with zero pointer collisions between storage, indexes, and register temporaries.
* **Positive**: Enables compile-time verification of head movement bounds.
* **Negative**: Requires bounded record payload sizes (default 8KB per record).
