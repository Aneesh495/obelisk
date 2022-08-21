"""
Obelisk Wire Protocol Codec (protocol.py)

Encodes client command frames and decodes RESP-compatible server responses.
"""

import socket
from typing import Any, List, Optional
from .exceptions import ObeliskProtocolError

class ProtocolCodec:
    """Handles serialization and deserialization of Obelisk wire frames."""

    @staticmethod
    def encode_command(*args) -> bytes:
        """Serializes command tokens into standard RESP or line-delimited format."""
        tokens = [str(a) for a in args]
        line = " ".join(tokens) + "\r\n"
        return line.encode("utf-8")

    @staticmethod
    def read_response(sock: socket.socket) -> Any:
        """Parses a structured response frame from the socket."""
        line = ProtocolCodec._read_line(sock)
        if not line:
            raise ObeliskProtocolError("Connection closed unexpectedly by server")

        prefix = line[0]
        payload = line[1:]

        if prefix == '+':
            # Simple string (e.g. +OK, +PONG)
            return payload

        elif prefix == '-':
            # Error frame
            raise ObeliskProtocolError(f"Server error: {payload}")

        elif prefix == ':':
            # Integer frame
            try:
                return int(payload)
            except ValueError:
                raise ObeliskProtocolError(f"Malformed integer response: {payload}")

        elif prefix == '$':
            # Bulk string frame
            try:
                length = int(payload)
            except ValueError:
                raise ObeliskProtocolError(f"Malformed bulk string length: {payload}")

            if length == -1:
                return None  # Key not found / Nil

            data = ProtocolCodec._read_exact(sock, length)
            # Read trailing CRLF
            crlf = ProtocolCodec._read_exact(sock, 2)
            if crlf != b"\r\n":
                raise ObeliskProtocolError("Expected CRLF after bulk string payload")

            return data.decode("utf-8", errors="replace")

        elif prefix == '*':
            # Array frame
            try:
                count = int(payload)
            except ValueError:
                raise ObeliskProtocolError(f"Malformed array count: {payload}")

            if count == -1:
                return None

            items: List[Any] = []
            for _ in range(count):
                items.append(ProtocolCodec.read_response(sock))
            return items

        else:
            raise ObeliskProtocolError(f"Unknown response frame prefix: {prefix}")

    @staticmethod
    def _read_line(sock: socket.socket) -> str:
        buf = bytearray()
        while True:
            b = sock.recv(1)
            if not b:
                break
            buf.extend(b)
            if len(buf) >= 2 and buf[-2:] == b"\r\n":
                return buf[:-2].decode("utf-8", errors="replace")
        return buf.decode("utf-8", errors="replace")

    @staticmethod
    def _read_exact(sock: socket.socket, n: int) -> bytes:
        buf = bytearray()
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise ObeliskProtocolError("Unexpected EOF while reading payload")
            buf.extend(chunk)
        return bytes(buf)
