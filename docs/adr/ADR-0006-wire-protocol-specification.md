# ADR-0006: Wire Protocol and RESP Compatibility

## Status
Accepted

## Context
A network database must offer predictable wire framing that is simple to parse from client drivers in any language without heavy third-party dependencies.

## Decision
We adopt the Redis Serialization Protocol (RESP v2) framing standard:
* Commands are parsed as whitespace-delimited tokens terminated by `\r\n`.
* Responses use standard RESP type markers (`+` simple string, `-` error, `:` integer, `$` bulk string, `*` array).
* Binary payloads are length-prefixed, ensuring zero ambiguity around embedded whitespace or line breaks.

## Consequences
* **Positive**: Enables interoperability with existing RESP tools, benchmarks, and debuggers.
* **Positive**: Deterministic, lightweight frame parsing with minimal CPU cycles.
