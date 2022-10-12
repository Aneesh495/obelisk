# ADR-0004: Write-Ahead Logging (WAL) on Tape

## Status
Accepted

## Context
ACID compliance requires atomicity: if an operation fails or if a transaction is aborted, the database must cleanly revert to its prior state with zero residual corruption.

## Decision
We implement a dedicated WAL segment directly in tape memory:
* Prior to updating or deleting any key, an undo journal entry is staged containing the key, previous value, and prior existence state.
* If `ROLLBACK` is issued, entries are popped in LIFO order and applied in reverse.
* If `COMMIT` is issued, the WAL journal pointer is reset to zero.

## Consequences
* **Positive**: Full rollback capability with zero disk I/O latency for in-memory transactions.
* **Positive**: Complete isolation of failed operations during client exceptions.
