# ADR-0002: Multi-Tier Execution Pipeline

## Status
Accepted

## Context
Raw interpretive execution of Brainfuck suffers from high dispatch overhead and redundant tape pointer oscillations. To achieve production-grade database throughput (>10,000 QPS), advanced compilation strategies are necessary.

## Decision
We implement a three-tier execution hierarchy:
* **Tier 0**: Direct threaded bytecode interpreter using GNU computed gotos (`goto *dispatch_table[op]`).
* **Tier 1**: SSA optimizing pass combining arithmetic coalescing, pointer shift coalescing, clear-loop folding, multiply-add lowering, and SIMD zero scanning.
* **Tier 2**: Dynamic native JIT code emission directly into executable memory pages for ARM64 and x86-64 architectures.

## Consequences
* **Positive**: Tier 1 yields an immediate ~4x performance speedup over raw interpretation.
* **Positive**: Native JIT minimizes host CPU instruction cache misses.
* **Negative**: Requires architecture-specific machine code emitters.
