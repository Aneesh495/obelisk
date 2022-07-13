#!/usr/bin/env python3
"""
Obelisk Brainfuck Disassembler (disasm.py)

Disassembles raw Brainfuck source files into human-readable intermediate
representation instructions, annotated basic blocks, and loop jump targets.
"""

import sys
import os
import argparse
from typing import List, Tuple

def disassemble_brainfuck(source: str) -> List[Tuple[int, str, str]]:
    """
    Parses Brainfuck characters and produces (offset, mnemonic, annotation).
    """
    clean_ops = [c for c in source if c in "+-><.,[]"]
    stack: List[int] = []
    jump_map = {}

    for idx, op in enumerate(clean_ops):
        if op == '[':
            stack.append(idx)
        elif op == ']':
            if stack:
                open_idx = stack.pop()
                jump_map[open_idx] = idx
                jump_map[idx] = open_idx

    result: List[Tuple[int, str, str]] = []
    i = 0
    while i < len(clean_ops):
        op = clean_ops[i]

        # Coalesce '+' and '-'
        if op in '+-':
            delta = 0
            j = i
            while j < len(clean_ops) and clean_ops[j] in '+-':
                delta += 1 if clean_ops[j] == '+' else -1
                j += 1
            mnemonic = f"ADD {delta:+d}"
            note = f"Mutate cell by {delta}"
            result.append((i, mnemonic, note))
            i = j
            continue

        # Coalesce '>' and '<'
        if op in '><':
            shift = 0
            j = i
            while j < len(clean_ops) and clean_ops[j] in '><':
                shift += 1 if clean_ops[j] == '>' else -1
                j += 1
            mnemonic = f"SHIFT {shift:+d}"
            note = f"Shift tape head by {shift} cells"
            result.append((i, mnemonic, note))
            i = j
            continue

        if op == '.':
            result.append((i, "PUTC", "Emit byte at head to stdout"))
        elif op == ',':
            result.append((i, "GETC", "Read byte from stdin into cell"))
        elif op == '[':
            target = jump_map.get(i, -1)
            result.append((i, f"JZ {target}", f"Jump to {target} if cell == 0"))
        elif op == ']':
            target = jump_map.get(i, -1)
            result.append((i, f"JNZ {target}", f"Jump to {target} if cell != 0"))

        i += 1

    return result

def main():
    parser = argparse.ArgumentParser(description="Obelisk Brainfuck Disassembler")
    parser.add_argument("source", help="Path to .bf file")
    args = parser.parse_args()

    if not os.path.exists(args.source):
        print(f"Error: File {args.source} not found", file=sys.stderr)
        sys.exit(1)

    with open(args.source, "r") as f:
        content = f.read()

    disassembly = disassemble_brainfuck(content)
    print(f"Disassembly of {args.source} ({len(disassembly)} instructions):")
    print(f"{'OFFSET':<8} {'MNEMONIC':<20} {'DESCRIPTION'}")
    print("-" * 60)
    for off, mnem, desc in disassembly[:100]:
        print(f"[{off:05d}]  {mnem:<20} {desc}")
    if len(disassembly) > 100:
        print(f"... ({len(disassembly) - 100} more instructions truncated)")

if __name__ == "__main__":
    main()
