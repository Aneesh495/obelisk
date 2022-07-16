#!/usr/bin/env python3
"""
Obelisk Tape Access Profiler & Memory Heatmap Visualizer (tape_profiler.py)

Simulates Brainfuck program execution while recording per-cell read/write
frequencies, tape head travel distances, and memory locality heatmaps.
"""

import sys
import os
import argparse
from typing import Dict, List

def profile_execution(source: str, max_steps: int = 1000000):
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

    tape_size = 65536
    tape = [0] * tape_size
    read_counts = [0] * tape_size
    write_counts = [0] * tape_size

    head = 0
    pc = 0
    steps = 0
    total_head_distance = 0
    max_head_index = 0

    while pc < len(clean_ops) and steps < max_steps:
        steps += 1
        op = clean_ops[pc]

        if op == '+':
            tape[head] = (tape[head] + 1) % 256
            write_counts[head] += 1
            pc += 1
        elif op == '-':
            tape[head] = (tape[head] - 1) % 256
            write_counts[head] += 1
            pc += 1
        elif op == '>':
            head = (head + 1) % tape_size
            total_head_distance += 1
            if head > max_head_index:
                max_head_index = head
            pc += 1
        elif op == '<':
            head = (head - 1 + tape_size) % tape_size
            total_head_distance += 1
            pc += 1
        elif op == '.':
            read_counts[head] += 1
            pc += 1
        elif op == ',':
            write_counts[head] += 1
            pc += 1
        elif op == '[':
            read_counts[head] += 1
            if tape[head] == 0:
                pc = jump_map.get(pc, len(clean_ops)) + 1
            else:
                pc += 1
        elif op == ']':
            read_counts[head] += 1
            if tape[head] != 0:
                pc = jump_map.get(pc, len(clean_ops)) + 1
            else:
                pc += 1

    return {
        "steps": steps,
        "head_distance": total_head_distance,
        "max_head": max_head_index,
        "read_counts": read_counts,
        "write_counts": write_counts
    }

def print_heatmap(profile_data, width: int = 64):
    max_idx = min(profile_data["max_head"] + 8, 256)
    writes = profile_data["write_counts"]
    max_w = max(writes[:max_idx]) if max_idx > 0 else 1
    if max_w == 0: max_w = 1

    chars = " .:-=+*#%@"
    print(f"\n=== Obelisk Turing Tape Access Heatmap (Cells 0..{max_idx}) ===")
    print(f"Total Steps: {profile_data['steps']:,} | Head Travel Distance: {profile_data['head_distance']:,} cells")
    print(f"High-Water Mark Cell: {profile_data['max_head']}")
    print("-" * 64)

    for i in range(0, max_idx, width):
        chunk = writes[i:i + width]
        row = ""
        for val in chunk:
            lvl = int((val / max_w) * (len(chars) - 1))
            row += chars[lvl]
        print(f"[{i:04d}-{min(i + width - 1, max_idx - 1):04d}]: {row}")
    print("-" * 64)

def main():
    parser = argparse.ArgumentParser(description="Obelisk Tape Access Profiler")
    parser.add_argument("source", help="Path to .bf file")
    parser.add_argument("--steps", type=int, default=500000, help="Max simulation steps")
    args = parser.parse_args()

    if not os.path.exists(args.source):
        print(f"Error: File {args.source} not found", file=sys.stderr)
        sys.exit(1)

    with open(args.source, "r") as f:
        content = f.read()

    data = profile_execution(content, max_steps=args.steps)
    print_heatmap(data)

if __name__ == "__main__":
    main()
