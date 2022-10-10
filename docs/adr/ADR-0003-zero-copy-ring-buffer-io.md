# ADR-0003: Zero-Copy Ring Buffer I/O Architecture

## Status
Accepted

## Context
Bridging standard POSIX socket network streams (`recv`/`send`) into Brainfuck virtual I/O callbacks (`getchar`/`putchar`) requires an efficient, thread-safe buffering mechanism to avoid socket starvation and excessive context switches.

## Decision
We implement a circular memory ring buffer (`ObeliskRingBuffer`) with mutex and condition variable synchronization:
* Inbound network data is written directly into the session ring buffer.
* The VM read callback reads directly from the ring without memory copying.
* Outbound responses serialize into the output ring buffer and flush asynchronously to client sockets.

## Consequences
* **Positive**: Thread-safe decouple between network event loops and VM execution threads.
* **Positive**: Minimizes memory allocation overhead per query.
