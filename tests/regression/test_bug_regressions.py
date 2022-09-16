"""
Regression Test Suite: Bug Ledger and Invariant Verification

This suite prevents regressions for all historical bugs discovered during the
engineering of Obelisk, verifying tape bounds, bracket balance, wire parsing,
and rollback semantics.
"""

import unittest
import subprocess
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "sdk", "python"))

from obelisk import ObeliskClient

class TestBugRegressions(unittest.TestCase):
    server_process = None
    port = 7396

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
        self.base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        self.bin_path = os.path.join(self.base_dir, "bin", "obelisk")
        self.client = ObeliskClient(port=self.port)
        self.client.connect()

    def tearDown(self):
        self.client.close()

    def test_bug_001_tape_head_negative_bounds_protection(self):
        """
        BUG-001: Ensure attempting to move tape pointer left past index 0
        returns OBELISK_ERR_OUT_OF_BOUNDS (code 1) rather than memory corruption.
        """
        tmp_bf = os.path.join(self.base_dir, "build", "bug001.bf")
        with open(tmp_bf, "w") as f:
            f.write("<+")  # Move left past head 0

        res = subprocess.run([self.bin_path, "run", tmp_bf], capture_output=True)
        self.assertEqual(res.returncode, 1)  # OBELISK_ERR_OUT_OF_BOUNDS

    def test_bug_002_bracket_matching_integrity(self):
        """
        BUG-002: Ensure all Brainfuck files in bf/ have exact 1:1 matching
        open and close brackets without any dangling loops.
        """
        bf_dir = os.path.join(self.base_dir, "bf")
        for root, _, files in os.walk(bf_dir):
            for file in files:
                if file.endswith(".bf"):
                    path = os.path.join(root, file)
                    with open(path, "r") as f:
                        content = f.read()
                    clean_ops = [c for c in content if c in "+-><.,[]"]
                    open_brackets = clean_ops.count('[')
                    close_brackets = clean_ops.count(']')
                    self.assertEqual(
                        open_brackets, close_brackets,
                        f"Bracket mismatch in {file}: {open_brackets} != {close_brackets}"
                    )

    def test_bug_007_keys_with_colons_and_underscores(self):
        """
        BUG-007: Wire parser should preserve colons, underscores, and dashes in keys.
        """
        complex_key = "user:session_token-v2:alpha_99"
        complex_val = '{"status":"active","ttl":3600}'
        self.client.set(complex_key, complex_val)
        retrieved = self.client.get(complex_key)
        self.assertEqual(retrieved, complex_val)

    def test_bug_008_rollback_nonexistent_key_cleanup(self):
        """
        BUG-008: Verify rolling back a newly inserted key completely removes it
        rather than leaving an occupied empty-string tombstone.
        """
        initial_count = self.client.count()
        with self.client.transaction() as tx:
            tx.set("transient:temp_123", "value")
            tx.rollback()

        # Count must remain exactly equal to initial_count
        self.assertEqual(self.client.count(), initial_count)
        self.assertFalse("transient:temp_123" in self.client)

if __name__ == "__main__":
    import time
    unittest.main()
