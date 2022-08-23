"""
Obelisk High-Level Client (client.py)

Provides a pythonic connection interface to Obelisk transactional database instances.
"""

import socket
from typing import Any, Dict, List, Optional
from .protocol import ProtocolCodec
from .exceptions import (
    ObeliskConnectionError,
    ObeliskKeyNotFoundError,
    ObeliskProtocolError
)
from .transaction import Transaction

class ObeliskClient:
    """Client connection for interacting with Obelisk database servers."""

    def __init__(self, host: str = "127.0.0.1", port: int = 7379, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self._sock: Optional[socket.socket] = None

    def connect(self) -> "ObeliskClient":
        """Establishes a TCP connection to the Obelisk server."""
        if self._sock is not None:
            return self

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(self.timeout)
            sock.connect((self.host, self.port))
            self._sock = sock
            return self
        except Exception as e:
            raise ObeliskConnectionError(f"Could not connect to Obelisk server at {self.host}:{self.port}: {e}")

    def close(self):
        """Closes the active connection."""
        if self._sock is not None:
            try:
                self._sock.close()
            except Exception:
                pass
            self._sock = None

    def __enter__(self) -> "ObeliskClient":
        return self.connect()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def execute_command(self, *args) -> Any:
        """Sends a command to the server and returns the parsed response."""
        if self._sock is None:
            self.connect()

        cmd_bytes = ProtocolCodec.encode_command(*args)
        try:
            self._sock.sendall(cmd_bytes)
            return ProtocolCodec.read_response(self._sock)
        except (socket.error, ObeliskProtocolError) as e:
            self.close()
            raise ObeliskConnectionError(f"Network error during command execution: {e}")

    def ping(self) -> str:
        """Pings the database instance to verify health."""
        return self.execute_command("PING")

    def set(self, key: str, value: str) -> bool:
        """Stores a key-value record."""
        res = self.execute_command("SET", key, value)
        return res == "OK"

    def get(self, key: str, default: Optional[str] = None) -> Optional[str]:
        """Retrieves the value for a given key."""
        res = self.execute_command("GET", key)
        return res if res is not None else default

    def delete(self, key: str) -> bool:
        """Deletes a record by key."""
        res = self.execute_command("DEL", key)
        return res == 1

    def exists(self, key: str) -> bool:
        """Checks if a key exists in the database."""
        return self.get(key) is not None

    def count(self) -> int:
        """Returns total active record count."""
        res = self.execute_command("COUNT")
        return int(res)

    def scan(self, prefix: str = "") -> List[str]:
        """Scans keys matching a specified prefix."""
        res = self.execute_command("SCAN", prefix)
        return res if isinstance(res, list) else []

    def stats(self) -> str:
        """Fetches runtime database statistics."""
        return self.execute_command("STATS")

    def compact(self) -> bool:
        """Triggers storage tape compaction."""
        res = self.execute_command("COMPACT")
        return "completed" in str(res).lower()

    def transaction(self) -> Transaction:
        """Creates an atomic ACID transaction context manager."""
        return Transaction(self)

    # Dictionary syntax overrides
    def __getitem__(self, key: str) -> str:
        val = self.get(key)
        if val is None:
            raise ObeliskKeyNotFoundError(f"Key not found: {key}")
        return val

    def __setitem__(self, key: str, value: str):
        self.set(key, value)

    def __delitem__(self, key: str):
        if not self.delete(key):
            raise ObeliskKeyNotFoundError(f"Key not found: {key}")

    def __contains__(self, key: str) -> bool:
        return self.exists(key)

    def __len__(self) -> int:
        return self.count()
