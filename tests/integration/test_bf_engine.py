"""
Integration Test Suite: Brainfuck Substrate Execution Across Execution Tiers
"""

import unittest
import subprocess
import os

class TestBrainfuckEngine(unittest.TestCase):
    def setUp(self):
        self.base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        self.bin_path = os.path.join(self.base_dir, "bin", "obelisk")
        self.tmp_bf = os.path.join(self.base_dir, "build", "test_sample.bf")
        os.makedirs(os.path.join(self.base_dir, "build"), exist_ok=True)

    def tearDown(self):
        if os.path.exists(self.tmp_bf):
            try:
                os.remove(self.tmp_bf)
            except Exception:
                pass

    def test_tier0_interpreter_hello(self):
        """Test Tier 0 bytecode interpreter running Hello World."""
        # Canonical Hello World
        src = ">++++++++[<+++++++++>-]<.>++++[<+++++++>-]<+.+++++++..+++.>>++++++[<+++++++>-]<++.------------.>++++++[<+++++++++>-]<+.<.+++.------.--------.>>>++++[<++++++++>-]<+."
        with open(self.tmp_bf, "w") as f:
            f.write(src)

        res = subprocess.run([self.bin_path, "run", self.tmp_bf, "--tier=0"], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertEqual(res.stdout, "Hello, World!")

    def test_tier1_optimizer_hello(self):
        """Test Tier 1 SSA optimized execution."""
        src = ">++++++++[<+++++++++>-]<.>++++[<+++++++>-]<+.+++++++..+++.>>++++++[<+++++++>-]<++.------------.>++++++[<+++++++++>-]<+.<.+++.------.--------.>>>++++[<++++++++>-]<+."
        with open(self.tmp_bf, "w") as f:
            f.write(src)

        res = subprocess.run([self.bin_path, "run", self.tmp_bf, "--tier=1"], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertEqual(res.stdout, "Hello, World!")

    def test_dump_ir(self):
        """Test dumping intermediate representation."""
        src = "+++>++[-]"
        with open(self.tmp_bf, "w") as f:
            f.write(src)

        res = subprocess.run([self.bin_path, "dump", self.tmp_bf], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertIn("ADD +3", res.stdout)
        self.assertIn("MOVE +1", res.stdout)
        self.assertIn("SET 0", res.stdout)

    def test_aot_transpile(self):
        """Test transpiling Brainfuck to compilable C source."""
        src = "+++++[>+++++<-]"
        with open(self.tmp_bf, "w") as f:
            f.write(src)

        out_c = os.path.join(self.base_dir, "build", "test_out.c")
        res = subprocess.run([self.bin_path, "transpile", self.tmp_bf, "-o", out_c], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0)
        self.assertTrue(os.path.exists(out_c))

        with open(out_c, "r") as f:
            c_code = f.read()
        self.assertIn("void obelisk_kernel_main", c_code)

if __name__ == "__main__":
    unittest.main()
