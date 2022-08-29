#!/usr/bin/env python3
"""
Obelisk Interactive Terminal REPL (obelisk_cli.py)

Rich terminal shell providing interactive database querying, transaction execution,
microsecond latency telemetry, and table formatting.
"""

import sys
import os
import time
import argparse

# Add SDK path to sys.path
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "sdk", "python"))

from obelisk import ObeliskClient, ObeliskError

# ANSI color codes
RESET = "\033[0m"
BOLD = "\033[1m"
CYAN = "\033[36m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
RED = "\033[31m"
MAGENTA = "\033[35m"
DIM = "\033[2m"

BANNER = f"""{CYAN}{BOLD}
   ___  ___  ___ _    ___ ___ _  __
  / _ \\/ _ )/ __/ /   / __/ _/ /_/ /
 / // / _  / _// /___/ _/ _\\/ // / 
 \\___/____/___/____/___/___/_/ \\_/  
{RESET}{DIM}Deterministic Transactional Database on Pure Brainfuck Substrate (v1.0.0){RESET}
Type {BOLD}'help'{RESET} for commands or {BOLD}'exit'{RESET} to quit.
"""

def format_table(headers, rows):
    """Renders tabular data in clean ASCII box format."""
    col_widths = [len(h) for h in headers]
    for row in rows:
        for idx, cell in enumerate(row):
            col_widths[idx] = max(col_widths[idx], len(str(cell)))

    sep = "+" + "+".join("-" * (w + 2) for w in col_widths) + "+"
    header_str = "|" + "|".join(f" {headers[i]:<{col_widths[i]}} " for i in range(len(headers))) + "|"

    out = [sep, header_str, sep]
    for row in rows:
        row_str = "|" + "|".join(f" {str(row[i]):<{col_widths[i]}} " for i in range(len(row))) + "|"
        out.append(row_str)
    out.append(sep)
    return "\n".join(out)

def run_repl(host: str, port: int):
    print(BANNER)
    client = ObeliskClient(host=host, port=port)
    try:
        client.connect()
        print(f"{GREEN}Connected to Obelisk instance at {host}:{port}{RESET}\n")
    except Exception as e:
        print(f"{RED}Error connecting to {host}:{port}: {e}{RESET}")
        print("Ensure the server daemon is running via 'bin/obelisk server'.\n")

    in_txn = False

    while True:
        try:
            prompt_prefix = f"{MAGENTA}(txn){RESET} " if in_txn else ""
            prompt = f"{prompt_prefix}{BOLD}{CYAN}obelisk:{port}>{RESET} "
            line = input(prompt).strip()
        except (KeyboardInterrupt, EOFError):
            print("\nExiting.")
            break

        if not line:
            continue

        if line.lower() in ("exit", "quit"):
            print("Goodbye.")
            break

        if line.lower() == "help":
            print(f"""
{BOLD}Available Commands:{RESET}
  {GREEN}PING{RESET}                      Test server connectivity
  {GREEN}SET <key> <value>{RESET}         Store key-value record
  {GREEN}GET <key>{RESET}                 Retrieve value by key
  {GREEN}DEL <key>{RESET}                 Delete record by key
  {GREEN}SCAN [prefix]{RESET}             List stored keys matching prefix
  {GREEN}COUNT{RESET}                     Get total active key count
  {GREEN}BEGIN{RESET}                     Begin an atomic ACID transaction
  {GREEN}COMMIT{RESET}                    Commit active transaction
  {GREEN}ROLLBACK{RESET}                  Rollback uncommitted changes
  {GREEN}STATS{RESET}                     Show storage tape and version statistics
  {GREEN}COMPACT{RESET}                   Trigger storage tape compaction
  {GREEN}EXIT / QUIT{RESET}               Exit interactive REPL
""")
            continue

        tokens = line.split()
        cmd = tokens[0].upper()

        t0 = time.perf_counter()
        try:
            res = client.execute_command(*tokens)
            elapsed_us = (time.perf_counter() - t0) * 1_000_000

            if cmd in ("BEGIN", "TXN_BEGIN"):
                in_txn = True
            elif cmd in ("COMMIT", "ROLLBACK", "TXN_COMMIT", "TXN_ROLLBACK"):
                in_txn = False

            if cmd == "SCAN" and isinstance(res, list):
                if res:
                    rows = [[idx + 1, k] for idx, k in enumerate(res)]
                    print(format_table(["#", "Key"], rows))
                else:
                    print(f"{DIM}(empty list or set){RESET}")
            elif res is None:
                print(f"{DIM}(nil){RESET}")
            else:
                print(f"{res}")

            print(f"{DIM}({elapsed_us:.1f} us){RESET}")

        except ObeliskError as e:
            elapsed_us = (time.perf_counter() - t0) * 1_000_000
            print(f"{RED}(error) {e}{RESET} {DIM}({elapsed_us:.1f} us){RESET}")
        except Exception as e:
            print(f"{RED}Network error: {e}{RESET}")
            try:
                client.connect()
            except Exception:
                pass

    client.close()

def main():
    parser = argparse.ArgumentParser(description="Obelisk Interactive CLI")
    parser.add_argument("--host", default="127.0.0.1", help="Server host")
    parser.add_argument("-p", "--port", type=int, default=7379, help="Server port")
    args = parser.parse_args()
    run_repl(args.host, args.port)

if __name__ == "__main__":
    main()
