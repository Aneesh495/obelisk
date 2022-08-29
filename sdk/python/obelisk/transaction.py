"""
Obelisk Transaction Context Manager (transaction.py)

Encapsulates atomic ACID transactions with automatic rollback on exception.
"""

from typing import Any, Optional
from .exceptions import ObeliskTransactionError

class Transaction:
    """Manages transactional state and rollback journal over an Obelisk session."""

    def __init__(self, client: Any):
        self.client = client
        self.is_active = False
        self.committed = False
        self.rolled_back = False

    def begin(self) -> "Transaction":
        """Issues TXN_BEGIN to the server."""
        res = self.client.execute_command("TXN_BEGIN")
        if res != "OK":
            raise ObeliskTransactionError(f"Failed to begin transaction: {res}")
        self.is_active = True
        return self

    def commit(self):
        """Commits all staged operations atomically."""
        if not self.is_active:
            raise ObeliskTransactionError("No active transaction to commit")
        res = self.client.execute_command("TXN_COMMIT")
        if res != "OK":
            raise ObeliskTransactionError(f"Commit failed: {res}")
        self.is_active = False
        self.committed = True

    def rollback(self):
        """Rolls back all changes made in this transaction."""
        if not self.is_active:
            return
        res = self.client.execute_command("TXN_ROLLBACK")
        self.is_active = False
        self.rolled_back = True

    def set(self, key: str, value: str):
        """Stages a SET operation within the transaction."""
        if not self.is_active:
            raise ObeliskTransactionError("Transaction is not active")
        self.client.set(key, value)

    def get(self, key: str) -> Optional[str]:
        """Reads a value within the transaction context."""
        return self.client.get(key)

    def delete(self, key: str) -> bool:
        """Stages a DEL operation within the transaction."""
        if not self.is_active:
            raise ObeliskTransactionError("Transaction is not active")
        return self.client.delete(key)

    def __enter__(self) -> "Transaction":
        return self.begin()

    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is not None:
            # An error occurred; rollback changes
            self.rollback()
        elif not self.committed and not self.rolled_back:
            # Commit automatically if exiting cleanly without explicit commit
            self.commit()
