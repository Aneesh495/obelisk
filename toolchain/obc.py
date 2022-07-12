#!/usr/bin/env python3
"""
Obelisk Macro-Assembler & Brainfuck Compiler (obc)

Translates structured intermediate assembly and high-level database operations
into deterministic, optimized Brainfuck instruction sequences over the 1-D tape.
"""

import sys
import os
import argparse
from typing import Dict, List, Tuple, Optional

class TapeMemoryModel:
    """
    Manages deterministic tape segmentation.
    Virtual register locations and scratch workspaces on the Turing tape.
    """
    # Tape layout offsets
    SCRATCH_BASE = 0
    REG_BASE = 16
    STACK_BASE = 64
    IO_IN_BASE = 256
    IO_OUT_BASE = 512
    WAL_BASE = 1024
    BTREE_BASE = 4096
    DATA_BASE = 16384

    def __init__(self):
        self.reg_map: Dict[str, int] = {}
        # Allocate standard virtual registers r0..r15
        for i in range(16):
            self.reg_map[f"r{i}"] = self.REG_BASE + i
        # Allocate scratch registers t0..t7
        for i in range(8):
            self.reg_map[f"t{i}"] = self.SCRATCH_BASE + i

        self.current_head = 0

    def get_cell(self, name_or_offset) -> int:
        if isinstance(name_or_offset, int):
            return name_or_offset
        if name_or_offset in self.reg_map:
            return self.reg_map[name_or_offset]
        try:
            return int(name_or_offset)
        except ValueError:
            raise ValueError(f"Unknown register or cell identifier: {name_or_offset}")

class BrainfuckEmitter:
    """
    Synthesizes minimal-cost Brainfuck instruction streams.
    Tracks head position to emit optimal shift sequences (< and >).
    """
    def __init__(self, memory_model: TapeMemoryModel):
        self.mem = memory_model
        self.ops: List[str] = []
        self.comments: List[str] = []

    def seek(self, target_cell: int) -> str:
        diff = target_cell - self.mem.current_head
        self.mem.current_head = target_cell
        if diff > 0:
            return ">" * diff
        elif diff < 0:
            return "<" * (-diff)
        return ""

    def clear(self, cell: int) -> str:
        s = self.seek(cell)
        s += "[-]"
        return s

    def set_val(self, cell: int, val: int) -> str:
        val = val % 256
        s = self.clear(cell)
        if val > 0:
            s += "+" * val
        return s

    def add_val(self, cell: int, delta: int) -> str:
        delta = delta % 256
        s = self.seek(cell)
        if delta <= 128:
            s += "+" * delta
        else:
            s += "-" * (256 - delta)
        return s

    def move_cell(self, dst: int, src: int) -> str:
        """Moves value from src to dst: dst += src; src = 0."""
        s = self.clear(dst)
        s += self.seek(src)
        diff = dst - src
        shift = (">" * diff) if diff > 0 else ("<" * (-diff))
        back = ("<" * diff) if diff > 0 else (">" * (-diff))
        s += f"[-{shift}+{back}]"
        self.mem.current_head = src
        return s

    def copy_cell(self, dst: int, src: int, temp: int) -> str:
        """Copies value from src to dst using temp cell: dst = src (src preserved)."""
        s = self.clear(dst)
        s += self.clear(temp)
        s += self.seek(src)

        d_dst = dst - src
        shift_dst = (">" * d_dst) if d_dst > 0 else ("<" * (-d_dst))
        back_dst = ("<" * d_dst) if d_dst > 0 else (">" * (-d_dst))

        d_tmp = temp - src
        shift_tmp = (">" * d_tmp) if d_tmp > 0 else ("<" * (-d_tmp))
        back_tmp = ("<" * d_tmp) if d_tmp > 0 else (">" * (-d_tmp))

        s += f"[-{shift_dst}+{back_dst}{shift_tmp}+{back_tmp}]"
        self.mem.current_head = src

        # Restore src from temp
        s += self.seek(temp)
        d_src = src - temp
        shift_src = (">" * d_src) if d_src > 0 else ("<" * (-d_src))
        back_src = ("<" * d_src) if d_src > 0 else (">" * (-d_src))
        s += f"[-{shift_src}+{back_src}]"
        self.mem.current_head = temp
        return s

    def multiply_cells(self, dst: int, src1: int, src2: int, t0: int, t1: int) -> str:
        """dst = src1 * src2."""
        s = self.clear(dst)
        s += self.copy_cell(t0, src1, t1)
        s += self.seek(t0)
        s += "["
        s += self.copy_cell(t1, src2, dst)
        s += self.seek(t1)
        d_dst = dst - t1
        shift_dst = (">" * d_dst) if d_dst > 0 else ("<" * (-d_dst))
        back_dst = ("<" * d_dst) if d_dst > 0 else (">" * (-d_dst))
        s += f"[-{shift_dst}+{back_dst}]"
        self.mem.current_head = t1
        s += self.seek(t0)
        s += "-]"
        return s

    def print_string(self, text: str, temp_cell: int) -> str:
        """Prints an ASCII string to stdout via virtual cell."""
        s = ""
        current_val = 0
        s += self.seek(temp_cell)
        s += "[-]"
        for ch in text:
            target_val = ord(ch)
            diff = (target_val - current_val) % 256
            if diff <= 128:
                s += "+" * diff
            else:
                s += "-" * (256 - diff)
            s += "."
            current_val = target_val
        s += "[-]"
        return s

def assemble_macro_source(lines: List[str]) -> str:
    """
    Parses macro-assembly lines and synthesizes verified Brainfuck output.
    """
    mem = TapeMemoryModel()
    emitter = BrainfuckEmitter(mem)
    output_parts: List[str] = []

    for line_idx, line in enumerate(lines, 1):
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("//"):
            continue

        tokens = line.split()
        cmd = tokens[0].lower()

        if cmd == "set":
            reg = mem.get_cell(tokens[1])
            val = int(tokens[2])
            output_parts.append(emitter.set_val(reg, val))

        elif cmd == "add":
            reg = mem.get_cell(tokens[1])
            val = int(tokens[2])
            output_parts.append(emitter.add_val(reg, val))

        elif cmd == "sub":
            reg = mem.get_cell(tokens[1])
            val = int(tokens[2])
            output_parts.append(emitter.add_val(reg, -val))

        elif cmd == "clear":
            reg = mem.get_cell(tokens[1])
            output_parts.append(emitter.clear(reg))

        elif cmd == "move":
            dst = mem.get_cell(tokens[1])
            src = mem.get_cell(tokens[2])
            output_parts.append(emitter.move_cell(dst, src))

        elif cmd == "copy":
            dst = mem.get_cell(tokens[1])
            src = mem.get_cell(tokens[2])
            tmp = mem.get_cell(tokens[3] if len(tokens) > 3 else "t0")
            output_parts.append(emitter.copy_cell(dst, src, tmp))

        elif cmd == "mult":
            dst = mem.get_cell(tokens[1])
            s1 = mem.get_cell(tokens[2])
            s2 = mem.get_cell(tokens[3])
            output_parts.append(emitter.multiply_cells(dst, s1, s2, mem.get_cell("t0"), mem.get_cell("t1")))

        elif cmd == "print":
            # Print literal string
            text = line.split(maxsplit=1)[1].strip()
            if text.startswith('"') and text.endswith('"'):
                text = text[1:-1]
            output_parts.append(emitter.print_string(text, mem.get_cell("t0")))

        elif cmd == "read":
            reg = mem.get_cell(tokens[1])
            s = emitter.seek(reg) + ","
            output_parts.append(s)

        elif cmd == "write":
            reg = mem.get_cell(tokens[1])
            s = emitter.seek(reg) + "."
            output_parts.append(s)

        elif cmd == "loop_start":
            reg = mem.get_cell(tokens[1])
            s = emitter.seek(reg) + "["
            output_parts.append(s)

        elif cmd == "loop_end":
            reg = mem.get_cell(tokens[1])
            s = emitter.seek(reg) + "]"
            output_parts.append(s)

        elif cmd == "raw":
            bf_chars = tokens[1]
            output_parts.append(bf_chars)

    return "".join(output_parts)

def format_brainfuck_code(raw_bf: str, line_length: int = 80, comment_header: Optional[str] = None) -> str:
    """
    Formats raw Brainfuck into clean, segmented, standard 80-character lines.
    """
    lines: List[str] = []
    if comment_header:
        for ch in comment_header.splitlines():
            lines.append(f"# {ch}")
        lines.append("")

    for i in range(0, len(raw_bf), line_length):
        lines.append(raw_bf[i:i + line_length])

    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser(description="Obelisk Brainfuck Macro-Assembler")
    parser.add_argument("input", help="Source assembly or script file")
    parser.add_argument("-o", "--output", help="Output Brainfuck file path")
    parser.add_argument("--wrap", type=int, default=80, help="Line wrap width")
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: Input file {args.input} not found", file=sys.stderr)
        sys.exit(1)

    with open(args.input, "r") as f:
        content = f.readlines()

    bf_code = assemble_macro_source(content)
    formatted = format_brainfuck_code(bf_code, line_length=args.wrap)

    if args.output:
        with open(args.output, "w") as f:
            f.write(formatted + "\n")
        print(f"Assembled {len(bf_code)} instructions to {args.output}")
    else:
        print(formatted)

if __name__ == "__main__":
    main()
