"""
Integration Test Suite: Obelisk TCP Server & Basic Operations
"""

import unittest
import subprocess
import time
import os
import sys

# Add SDK path
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "sdk", "python"))

from obelisk import ObeliskClient

class TestObeliskServer(unittest.TestCase):
    server_process = None
    port = 7399

    @classmethod
    def setUpClass(cls):
        base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        bin_path = os.path.join(base_dir, "bin", "obelisk")
        
        # Launch server daemon on custom test port
        cls.server_process = subprocess.Popen(
            [bin_path, "server", f"--port={cls.port}"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        # Give server time to bind and listen
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

    def test_ping(self):
        """Verify server responds to PING with PONG."""
        res = self.client.ping()
        self.assertEqual(res, "PONG")

    def test_set_and_get(self):
        """Test storing and reading back key-value pairs."""
        self.assertTrue(self.client.set("alpha", "12345"))
        val = self.client.get("alpha")
        self.assertEqual(val, "12345")

    def test_get_nonexistent(self):
        """Verify reading nonexistent key returns None."""
        val = self.client.get("nonexistent_key_999")
        self.assertIsNone(val)

    def test_delete(self):
        """Test record deletion."""
        self.client.set("beta", "to_be_deleted")
        self.assertTrue(self.client.exists("beta"))
        self.assertTrue(self.client.delete("beta"))
        self.assertFalse(self.client.exists("beta"))

    def test_count_and_scan(self):
        """Test counting records and prefix scanning."""
        self.client.set("user:1", "Alice")
        self.client.set("user:2", "Bob")
        self.client.set("item:1", "Widget")

        count = self.client.count()
        self.assertGreaterEqual(count, 3)

        user_keys = self.client.scan(prefix="user:")
        self.assertIn("user:1", user_keys)
        self.assertIn("user:2", user_keys)
        self.assertNotIn("item:1", user_keys)

    def test_dict_interface(self):
        """Test pythonic dictionary syntax."""
        self.client["config:theme"] = "dark"
        self.assertEqual(self.client["config:theme"], "dark")
        self.assertTrue("config:theme" in self.client)
        del self.client["config:theme"]
        self.assertFalse("config:theme" in self.client)

if __name__ == "__main__":
    unittest.main()
