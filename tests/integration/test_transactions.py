"""
Integration Test Suite: ACID Transaction Guarantees and Rollback Verification
"""

import unittest
import subprocess
import time
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "sdk", "python"))

from obelisk import ObeliskClient, ObeliskTransactionError

class TestObeliskTransactions(unittest.TestCase):
    server_process = None
    port = 7398

    @classmethod
    def setUpClass(cls):
        base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        bin_path = os.path.join(base_dir, "bin", "obelisk")
        
        cls.server_process = subprocess.Popen(
            [bin_path, "server", f"--port={cls.port}"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(0.3)

    @classmethod
    def tearDownClass(cls):
        if cls.server_process:
            cls.server_process.terminate()
            cls.server_process.wait()

    def setUp(self):
        self.client = ObeliskClient(port=self.port)
        self.client.connect()

    def tearDown(self):
        self.client.close()

    def test_atomic_commit(self):
        """Verify changes persist after explicit commit."""
        with self.client.transaction() as tx:
            tx.set("txn:commit_key", "committed_value")
            tx.commit()

        self.assertEqual(self.client.get("txn:commit_key"), "committed_value")

    def test_atomic_rollback_insert(self):
        """Verify new keys inserted during a rolled-back transaction disappear."""
        with self.client.transaction() as tx:
            tx.set("txn:rollback_key", "should_disappear")
            tx.rollback()

        self.assertIsNone(self.client.get("txn:rollback_key"))

    def test_rollback_reverts_overwrite(self):
        """Verify modifying an existing key reverts to previous value on rollback."""
        self.client.set("txn:existing_key", "initial_stable_value")
        self.assertEqual(self.client.get("txn:existing_key"), "initial_stable_value")

        with self.client.transaction() as tx:
            tx.set("txn:existing_key", "mutated_value")
            self.assertEqual(tx.get("txn:existing_key"), "mutated_value")
            tx.rollback()

        # Must have reverted to initial_stable_value
        self.assertEqual(self.client.get("txn:existing_key"), "initial_stable_value")

    def test_rollback_restores_deletion(self):
        """Verify deleting a key in a rolled-back transaction restores the key."""
        self.client.set("txn:keep_me", "preserved_data")
        self.assertEqual(self.client.get("txn:keep_me"), "preserved_data")

        with self.client.transaction() as tx:
            tx.delete("txn:keep_me")
            self.assertIsNone(tx.get("txn:keep_me"))
            tx.rollback()

        # Must be restored
        self.assertEqual(self.client.get("txn:keep_me"), "preserved_data")

    def test_exception_triggers_auto_rollback(self):
        """Verify an unhandled exception inside transaction context triggers automatic rollback."""
        self.client.set("txn:safe_key", "original_state")

        try:
            with self.client.transaction() as tx:
                tx.set("txn:safe_key", "corrupted_state")
                tx.set("txn:new_unwanted", "do_not_persist")
                raise RuntimeError("Simulated crash inside business logic")
        except RuntimeError:
            pass

        self.assertEqual(self.client.get("txn:safe_key"), "original_state")
        self.assertIsNone(self.client.get("txn:new_unwanted"))

if __name__ == "__main__":
    unittest.main()
