#include "obelisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline uint64_t bench_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int main(void) {
    printf("========================================================\n");
    printf("         Obelisk VM Execution Tier Micro-Benchmarks      \n");
    printf("========================================================\n\n");

    /* Program: Intensive arithmetic and nested loop */
    const char *benchmark_code = "+++++[>+++++[>+++++<-]<-]>>[-]";

    ObeliskProgram *unopt_prog = NULL;
    obelisk_parse_source(benchmark_code, strlen(benchmark_code), &unopt_prog);

    ObeliskProgram *opt_prog = NULL;
    obelisk_parse_and_optimize(benchmark_code, strlen(benchmark_code), OPT_PASS_ALL, &opt_prog);

    /* 1. Benchmark Tier 0: Direct Threaded Interpreter (Unoptimized) */
    {
        ObeliskVM vm;
        obelisk_vm_init(&vm, 1024, OBELISK_TIER_INTERPRETER);
        uint64_t t0 = bench_time_ns();
        for (int i = 0; i < 10000; ++i) {
            obelisk_tape_reset(&vm.tape);
            obelisk_vm_run_program(&vm, unopt_prog);
        }
        uint64_t t1 = bench_time_ns();
        printf("Tier 0 (Bytecode Interpreter):       %8.2f ms (10,000 iterations)\n", (double)(t1 - t0) / 1e6);
        obelisk_vm_free(&vm);
    }

    /* 2. Benchmark Tier 1: SSA-Optimized Bytecode Interpreter */
    {
        ObeliskVM vm;
        obelisk_vm_init(&vm, 1024, OBELISK_TIER_OPTIMIZED);
        uint64_t t0 = bench_time_ns();
        for (int i = 0; i < 10000; ++i) {
            obelisk_tape_reset(&vm.tape);
            obelisk_vm_run_program(&vm, opt_prog);
        }
        uint64_t t1 = bench_time_ns();
        printf("Tier 1 (SSA-Optimized Bytecode):     %8.2f ms (10,000 iterations)\n", (double)(t1 - t0) / 1e6);
        obelisk_vm_free(&vm);
    }

    /* 3. Benchmark Tier 2: Native JIT Compiler */
    {
        if (obelisk_jit_detect_host_target() != JIT_TARGET_NONE) {
            ObeliskVM vm;
            obelisk_vm_init(&vm, 1024, OBELISK_TIER_JIT);
            uint64_t t0 = bench_time_ns();
            for (int i = 0; i < 10000; ++i) {
                obelisk_tape_reset(&vm.tape);
                obelisk_vm_run_source(&vm, benchmark_code, strlen(benchmark_code));
            }
            uint64_t t1 = bench_time_ns();
            printf("Tier 2 (Native JIT Machine Code):    %8.2f ms (10,000 iterations)\n", (double)(t1 - t0) / 1e6);
            obelisk_vm_free(&vm);
        }
    }

    obelisk_program_free(unopt_prog);
    obelisk_program_free(opt_prog);

    printf("\nMicro-benchmark run finished successfully.\n");
    return 0;
}
