"""
Obelisk End-to-End Query & Throughput Benchmark Suite (bench_queries.py)

Measures queries-per-second (QPS) throughput and latency percentiles (p50, p95, p99)
across SET, GET, and ACID transaction workloads over TCP.
"""

import os
import sys
import time
import subprocess
import statistics

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "sdk", "python"))
from obelisk import ObeliskClient

def run_benchmarks():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bin_path = os.path.join(base_dir, "bin", "obelisk")
    port = 7395

    print("========================================================")
    print("       Obelisk End-to-End TCP Query Performance         ")
    print("========================================================\n")

    srv = subprocess.Popen([bin_path, "server", f"--port={port}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.3)

    try:
        client = ObeliskClient(port=port)
        client.connect()

        # 1. SET Benchmark
        iterations = 5000
        set_latencies = []
        t0 = time.perf_counter()
        for i in range(iterations):
            k = f"bench:key:{i}"
            v = f"payload_value_{i}"
            s_t = time.perf_counter()
            client.set(k, v)
            set_latencies.append((time.perf_counter() - s_t) * 1_000_000)
        total_time = time.perf_counter() - t0
        set_qps = iterations / total_time

        print(f"SET Throughput ({iterations} ops):")
        print(f"  QPS:                   {set_qps:,.0f} queries/sec")
        print(f"  Latency p50:           {statistics.median(set_latencies):.1f} us")
        print(f"  Latency p95:           {statistics.quantiles(set_latencies, n=20)[18]:.1f} us")
        print(f"  Latency p99:           {statistics.quantiles(set_latencies, n=100)[98]:.1f} us\n")

        # 2. GET Benchmark
        get_latencies = []
        t0 = time.perf_counter()
        for i in range(iterations):
            k = f"bench:key:{i}"
            g_t = time.perf_counter()
            val = client.get(k)
            get_latencies.append((time.perf_counter() - g_t) * 1_000_000)
        total_time = time.perf_counter() - t0
        get_qps = iterations / total_time

        print(f"GET Throughput ({iterations} ops):")
        print(f"  QPS:                   {get_qps:,.0f} queries/sec")
        print(f"  Latency p50:           {statistics.median(get_latencies):.1f} us")
        print(f"  Latency p95:           {statistics.quantiles(get_latencies, n=20)[18]:.1f} us")
        print(f"  Latency p99:           {statistics.quantiles(get_latencies, n=100)[98]:.1f} us\n")

        # 3. Transaction Benchmark
        txn_iterations = 1000
        txn_latencies = []
        t0 = time.perf_counter()
        for i in range(txn_iterations):
            tx_t = time.perf_counter()
            with client.transaction() as tx:
                tx.set(f"txn:bench:{i}", "val")
                tx.get(f"txn:bench:{i}")
                tx.commit()
            txn_latencies.append((time.perf_counter() - tx_t) * 1_000_000)
        total_time = time.perf_counter() - t0
        txn_qps = txn_iterations / total_time

        print(f"Transactional Throughput ({txn_iterations} atomic txns):")
        print(f"  TPS:                   {txn_qps:,.0f} transactions/sec")
        print(f"  Latency p50:           {statistics.median(txn_latencies):.1f} us")
        print(f"  Latency p95:           {statistics.quantiles(txn_latencies, n=20)[18]:.1f} us\n")

        client.close()

    finally:
        srv.terminate()
        srv.wait()

    print("End-to-end benchmark suite executed cleanly.\n")

if __name__ == "__main__":
    run_benchmarks()
