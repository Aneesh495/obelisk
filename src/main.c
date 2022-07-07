#include "obelisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog_name) {
    printf("Obelisk: Deterministic Transactional Database & Optimizing JIT Substrate (v%s)\n\n", OBELISK_VERSION_STRING);
    printf("Usage:\n");
    printf("  %s run <file.bf> [--tier=0|1|2] [--steps=N]      Execute Brainfuck source\n", prog_name);
    printf("  %s server [--port=PORT] [--tier=0|1|2]           Launch transactional TCP server daemon\n", prog_name);
    printf("  %s transpile <file.bf> [-o output.c]             Transpile Brainfuck to optimized C code\n", prog_name);
    printf("  %s optimize <file.bf>                            Run SSA optimizer and display statistics\n", prog_name);
    printf("  %s dump <file.bf>                                Dump lowered IR intermediate representation\n", prog_name);
    printf("  %s version                                       Display version metadata\n", prog_name);
}

static char *read_file_to_string(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read_bytes = fread(buf, 1, (size_t)sz, f);
    buf[read_bytes] = '\0';
    fclose(f);
    if (out_len) *out_len = read_bytes;
    return buf;
}

static int stdio_read(void *u) {
    (void)u;
    return getchar();
}

static int stdio_write(uint8_t b, void *u) {
    (void)u;
    return putchar((int)b) == EOF ? -1 : 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0) {
        printf("obelisk version %s\n", OBELISK_VERSION_STRING);
        printf("Architecture Target: %s\n",
               obelisk_jit_detect_host_target() == JIT_TARGET_ARM64 ? "ARM64 (Apple Silicon / AArch64)" :
               obelisk_jit_detect_host_target() == JIT_TARGET_X86_64 ? "x86-64 / AMD64" : "Interpreter Only");
        return 0;
    }

    if (strcmp(cmd, "server") == 0) {
        int port = OBELISK_DEFAULT_PORT;
        ObeliskTier tier = OBELISK_TIER_OPTIMIZED;

        for (int i = 2; i < argc; ++i) {
            if (strncmp(argv[i], "--port=", 7) == 0) {
                port = atoi(argv[i] + 7);
            } else if (strcmp(argv[i], "--tier=0") == 0) {
                tier = OBELISK_TIER_INTERPRETER;
            } else if (strcmp(argv[i], "--tier=1") == 0) {
                tier = OBELISK_TIER_OPTIMIZED;
            } else if (strcmp(argv[i], "--tier=2") == 0) {
                tier = OBELISK_TIER_JIT;
            }
        }

        ObeliskServerConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.port = port;
        cfg.tape_size = OBELISK_DEFAULT_TAPE_SIZE;
        cfg.tier = tier;
        cfg.verbose = true;

        ObeliskServer server;
        if (!obelisk_server_init(&server, &cfg)) {
            fprintf(stderr, "Error: Failed to initialize Obelisk server on port %d\n", port);
            return 1;
        }

        obelisk_server_start(&server);
        obelisk_server_free(&server);
        return 0;
    }

    if (strcmp(cmd, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'run' requires a Brainfuck source file path\n");
            return 1;
        }
        const char *filepath = argv[2];
        ObeliskTier tier = OBELISK_TIER_OPTIMIZED;

        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--tier=0") == 0) {
                tier = OBELISK_TIER_INTERPRETER;
            } else if (strcmp(argv[i], "--tier=1") == 0) {
                tier = OBELISK_TIER_OPTIMIZED;
            } else if (strcmp(argv[i], "--tier=2") == 0) {
                tier = OBELISK_TIER_JIT;
            }
        }

        size_t source_len = 0;
        char *source = read_file_to_string(filepath, &source_len);
        if (!source) {
            fprintf(stderr, "Error: Cannot open source file: %s\n", filepath);
            return 1;
        }

        ObeliskVM vm;
        if (obelisk_vm_init(&vm, OBELISK_DEFAULT_TAPE_SIZE, tier) != OBELISK_OK) {
            free(source);
            fprintf(stderr, "Error: Failed to initialize virtual machine\n");
            return 1;
        }

        obelisk_vm_set_io(&vm, stdio_read, stdio_write, NULL);
        ObeliskStatus st = obelisk_vm_run_source(&vm, source, source_len);

        free(source);
        obelisk_vm_free(&vm);

        if (st != OBELISK_OK) {
            fprintf(stderr, "\nExecution exited with error code: %d\n", (int)st);
            return (int)st;
        }
        return 0;
    }

    if (strcmp(cmd, "optimize") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'optimize' requires a Brainfuck source file\n");
            return 1;
        }
        size_t source_len = 0;
        char *source = read_file_to_string(argv[2], &source_len);
        if (!source) {
            fprintf(stderr, "Error: Cannot open file: %s\n", argv[2]);
            return 1;
        }

        ObeliskProgram *prog = NULL;
        ObeliskOptStats stats;
        if (!obelisk_parse_and_optimize(source, source_len, OPT_PASS_ALL, &prog)) {
            free(source);
            fprintf(stderr, "Error: Failed to parse and optimize program\n");
            return 1;
        }

        obelisk_optimize_program(prog, OPT_PASS_ALL, &stats);
        printf("Optimization Report for %s:\n", argv[2]);
        printf("  Source Length:                  %zu bytes\n", source_len);
        printf("  Raw Instructions:               %zu\n", stats.instructions_before);
        printf("  Optimized Instructions:         %zu\n", stats.instructions_after);
        printf("  Contiguous Ops Coalesced:       %zu\n", stats.contiguous_ops_coalesced);
        printf("  Clear Loops Folded:             %zu\n", stats.clear_loops_folded);
        printf("  Multiply-Adds Folded:           %zu\n", stats.multiply_adds_folded);
        printf("  Zero Scans Folded:              %zu\n", stats.zero_scans_folded);
        if (stats.instructions_before > 0) {
            double reduction = 100.0 * (1.0 - (double)stats.instructions_after / (double)stats.instructions_before);
            printf("  Instruction Count Reduction:    %.2f%%\n", reduction);
        }

        obelisk_program_free(prog);
        free(source);
        return 0;
    }

    if (strcmp(cmd, "dump") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'dump' requires a Brainfuck source file\n");
            return 1;
        }
        size_t source_len = 0;
        char *source = read_file_to_string(argv[2], &source_len);
        if (!source) {
            fprintf(stderr, "Error: Cannot open file: %s\n", argv[2]);
            return 1;
        }

        ObeliskProgram *prog = NULL;
        if (!obelisk_parse_and_optimize(source, source_len, OPT_PASS_ALL, &prog)) {
            free(source);
            fprintf(stderr, "Error: Failed to parse Brainfuck source\n");
            return 1;
        }

        obelisk_program_dump(prog);
        obelisk_program_free(prog);
        free(source);
        return 0;
    }

    if (strcmp(cmd, "transpile") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'transpile' requires a Brainfuck source file\n");
            return 1;
        }
        const char *out_path = NULL;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            }
        }

        size_t source_len = 0;
        char *source = read_file_to_string(argv[2], &source_len);
        if (!source) {
            fprintf(stderr, "Error: Cannot open file: %s\n", argv[2]);
            return 1;
        }

        ObeliskProgram *prog = NULL;
        if (!obelisk_parse_and_optimize(source, source_len, OPT_PASS_ALL, &prog)) {
            free(source);
            fprintf(stderr, "Error: Failed to parse Brainfuck source\n");
            return 1;
        }

        FILE *out_f = out_path ? fopen(out_path, "w") : stdout;
        if (!out_f) {
            obelisk_program_free(prog);
            free(source);
            fprintf(stderr, "Error: Cannot open output path: %s\n", out_path);
            return 1;
        }

        ObeliskAotOptions opts;
        memset(&opts, 0, sizeof(opts));
        opts.function_name = "obelisk_kernel_main";
        opts.tape_size = OBELISK_DEFAULT_TAPE_SIZE;

        obelisk_aot_transpile_to_c(prog, &opts, out_f);
        if (out_path) {
            fclose(out_f);
            printf("Transpiled C source written to %s\n", out_path);
        }

        obelisk_program_free(prog);
        free(source);
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
