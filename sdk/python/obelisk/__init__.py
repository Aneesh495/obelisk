"""
Obelisk Python Client Driver

High-performance client SDK for communicating with Obelisk database instances
over standard TCP wire connections. Supports transactions and connection pooling.
"""

from .client import ObeliskClient
from .transaction import Transaction
from .exceptions import (
    ObeliskError,
    ObeliskConnectionError,
    ObeliskProtocolError,
    ObeliskTransactionError,
    ObeliskKeyNotFoundError
)

__version__ = "1.0.0"
__all__ = [
    "ObeliskClient",
    "Transaction",
    "ObeliskError",
    "ObeliskConnectionError",
    "ObeliskProtocolError",
    "ObeliskTransactionError",
    "ObeliskKeyNotFoundError"
]
