"""
Integration Test Suite: Raw Wire Protocol and RESP Frame Conformance
"""

import unittest
import socket
import subprocess
import time
import os

class TestWireProtocol(unittest.TestCase):
    server_process = None
    port = 7397

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
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect(("127.0.0.1", self.port))

    def tearDown(self):
        self.sock.close()

    def test_raw_ping(self):
        """Verify raw PING yields +PONG\\r\\n."""
        self.sock.sendall(b"PING\r\n")
        resp = self.sock.recv(1024)
        self.assertEqual(resp, b"+PONG\r\n")

    def test_raw_set_and_get(self):
        """Verify raw SET and GET yield exact RESP frames."""
        self.sock.sendall(b"SET wire:msg Hello\r\n")
        resp = self.sock.recv(1024)
        self.assertEqual(resp, b"+OK\r\n")

        self.sock.sendall(b"GET wire:msg\r\n")
        resp = self.sock.recv(1024)
        self.assertEqual(resp, b"$5\r\nHello\r\n")

    def test_raw_get_missing(self):
        """Verify reading missing key yields $-1\\r\\n (Nil)."""
        self.sock.sendall(b"GET nonexistent_404\r\n")
        resp = self.sock.recv(1024)
        self.assertEqual(resp, b"$-1\r\n")

    def test_raw_delete(self):
        """Verify deleting key yields integer :1\\r\\n."""
        self.sock.sendall(b"SET wire:del Temp\r\n")
        self.sock.recv(1024)

        self.sock.sendall(b"DEL wire:del\r\n")
        resp = self.sock.recv(1024)
        self.assertEqual(resp, b":1\r\n")

        self.sock.sendall(b"DEL wire:del\r\n")
        resp = self.sock.recv(1024)
        self.assertEqual(resp, b":0\r\n")

if __name__ == "__main__":
    unittest.main()
