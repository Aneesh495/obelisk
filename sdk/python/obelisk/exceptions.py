"""
Obelisk Client Exception Hierarchy
"""

class ObeliskError(Exception):
    """Base exception for all Obelisk client errors."""
    pass

class ObeliskConnectionError(ObeliskError):
    """Raised when connection to the Obelisk server daemon fails."""
    pass

class ObeliskProtocolError(ObeliskError):
    """Raised when an invalid or malformed wire protocol response is received."""
    pass

class ObeliskTransactionError(ObeliskError):
    """Raised when a transactional operation fails or is aborted."""
    pass

class ObeliskKeyNotFoundError(ObeliskError):
    """Raised when a requested key does not exist in the storage engine."""
    pass
