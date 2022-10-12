# ADR-0005: ARM64 JIT Register Allocation Strategy

## Status
Accepted

## Context
Brainfuck execution is dominated by head dereferencing (`tape[head]`) and pointer movement (`head += delta`). Naive register assignment that loads and stores the tape pointer from memory on every instruction incurs severe memory pressure.

## Decision
On ARM64 (Apple Silicon / AArch64):
* Callee-saved register `x19` is pinned as the live tape pointer (`uint8_t *tape_ptr`).
* Callee-saved register `x20` holds the execution context pointer.
* Temporary scratch register `w8` is used for 8-bit cell loads and arithmetic mutations (`ldrb`/`strb`).
* Pointer movements translate to direct `add x19, x19, #imm` or `sub x19, x19, #imm`.

## Consequences
* **Positive**: The active tape head stays pinned in hardware CPU registers across the entire program.
* **Positive**: Eliminates memory spills for tape address computation.
