# Obelisk Verification, Invariant Proofs, and Quality Assurance

## 1. Quality Assurance Architecture

Obelisk enforces a verification regime consisting of:
1. **C Unit Test Suites**: Memory bounds checking, bytecode interpreter verification, optimizer loop folding, JIT dynamic compilation, and thread-safe ring buffers.
2. **Integration Verification**: Multi-client TCP socket server, Python driver query building, and command pipeline execution.
3. **Transaction ACID Tests**: Atomic commit durability, rollback of inserts, rollbacks of updates, and exception-triggered auto-rollback.
4. **Fuzzing Harness**: Robustness testing against randomized, malformed, and truncated byte streams.
5. **Historical Bug Regression Ledger**: Dedicated unit tests ensuring previously identified defects cannot resurface.

---

## 2. Invariant Proofs

### Invariant 1: Tape Pointer Safety
* **Formal Guarantee**: For all execution steps `t`, `0 <= Head_t < TapeSize`.
* **Verification**: Bounds check enforced at all pointer movement opcodes (`IR_MOVE`, `IR_SCAN_FWD`, `IR_SCAN_BCK`). Any violation raises `OBELISK_ERR_OUT_OF_BOUNDS`.

### Invariant 2: Bracket Balance & Loop Termination
* **Formal Guarantee**: Every opening bracket `[` has a unique matching closing bracket `]`.
* **Verification**: Verified statically during AST scanning. Unmatched brackets fail compilation with `OBELISK_ERR_UNMATCHED_BRACKET`.

### Invariant 3: Transaction Atomicity
* **Formal Guarantee**: A sequence of mutations in a transaction block either fully commits or leaves storage in its exact pre-transaction state.
* **Verification**: Prior to any write, an undo log record is staged in the WAL journal. On `ROLLBACK`, entries are reverted in reverse order.

---

## 3. Bug Regression Ledger

| Bug ID | Component | Defect Description | Resolution & Test Case |
| :--- | :--- | :--- | :--- |
| `BUG-001` | Tape Memory | Moving tape pointer left past cell 0 wrapped around in naive implementations. | Enforced strict bounds check raising `OBELISK_ERR_OUT_OF_BOUNDS` (`test_bug_001`). |
| `BUG-002` | Generator | Slicing Brainfuck code across arbitrary line lengths cut loop brackets in half. | Enforced integral balanced unit boundaries and padding checks (`test_bug_002`). |
| `BUG-003` | Compiler Headers | Missing typedef for `struct ObeliskProgram` caused header compilation errors. | Added explicit typedef in `include/obelisk/ir.h`. |
| `BUG-004` | AOT Transpiler | Missing `#include "obelisk/vm.h"` caused undeclared constant error. | Included `obelisk/vm.h` in `src/aot/aot_transpiler.c`. |
| `BUG-005` | Build Rules | Root `/lib/` rule in `.gitignore` shadowed `bf/lib` source modules. | Refined `.gitignore` path to avoid matching source subdirectories. |
| `BUG-006` | IR Interfaces | `obelisk_parse_source` was not declared in public header. | Declared prototype in `include/obelisk/ir.h`. |
| `BUG-007` | Wire Parser | Keys with colons or underscores were truncated by strict alpha tokenizers. | Replaced delimiter parsing with space-tokenized line scanner (`test_bug_007`). |
| `BUG-008` | WAL Engine | Rolling back a newly inserted key left an empty string placeholder. | Updated rollback logic to decrement active count and clear occupied flag (`test_bug_008`). |
