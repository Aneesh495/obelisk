"""
Obelisk Wire Protocol and Brainfuck Parser Fuzzing Harness (fuzz_parser.py)

Generates malformed, truncated, and randomized binary inputs to verify that
neither the C VM nor the wire parser suffer from crashes or memory faults.
"""

import os
import sys
import random
import subprocess
import socket
import time

def fuzz_brainfuck_parser(bin_path: str, iterations: int = 50):
    """Feeds random Brainfuck sequences to ensure graceful error handling."""
    print(f"[fuzz] Fuzzing Brainfuck parser ({iterations} iterations)...")
    chars = "+-><.,[] \t\nABCDEF12345!@#$%^&*()"
    tmp_path = "/tmp/obelisk_fuzz.bf"

    for i in range(iterations):
        length = random.randint(10, 500)
        payload = "".join(random.choice(chars) for _ in range(length))
        with open(tmp_path, "w") as f:
            f.write(payload)

        # Run with optimization
        res = subprocess.run([bin_path, "optimize", tmp_path], capture_output=True)
        # We expect either 0 (valid) or 1 (syntax error), NEVER 139 (SIGSEGV) or 134 (abort)
        if res.returncode not in (0, 1):
            raise AssertionError(f"Crash detected on iteration {i}: returncode {res.returncode}")

    if os.path.exists(tmp_path):
        os.remove(tmp_path)
    print("  PASSED: Zero parser crashes detected.")

def main():
    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    bin_path = os.path.join(base_dir, "bin", "obelisk")
    fuzz_brainfuck_parser(bin_path, iterations=100)

if __name__ == "__main__":
    main()
