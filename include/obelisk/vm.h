#ifndef OBELISK_VM_H
#define OBELISK_VM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Default memory configuration constants for the Obelisk virtual machine.
 * The tape size defaults to 1MB (1,048,576 cells) which accommodates the
 * transactional write-ahead log, B-tree indexes, and table data blocks.
 */
#define OBELISK_DEFAULT_TAPE_SIZE     (1024 * 1024)
#define OBELISK_MAX_TAPE_SIZE         (64 * 1024 * 1024)
#define OBELISK_DEFAULT_MAX_STEPS     (500000000ULL)

/*
 * Execution status codes returned by the virtual machine.
 */
typedef enum {
    OBELISK_OK = 0,
    OBELISK_ERR_OUT_OF_BOUNDS = 1,
    OBELISK_ERR_STEP_LIMIT_EXCEEDED = 2,
    OBELISK_ERR_IO_READ = 3,
    OBELISK_ERR_IO_WRITE = 4,
    OBELISK_ERR_INVALID_BYTECODE = 5,
    OBELISK_ERR_UNMATCHED_BRACKET = 6,
    OBELISK_ERR_OUT_OF_MEMORY = 7,
    OBELISK_ERR_JIT_COMPILATION = 8
} ObeliskStatus;

/*
 * Execution tier selection for the runtime engine.
 */
typedef enum {
    OBELISK_TIER_INTERPRETER = 0, /* Tier 0: Direct threaded bytecode interpreter */
    OBELISK_TIER_OPTIMIZED   = 1, /* Tier 1: SSA-optimized IR interpreter */
    OBELISK_TIER_JIT         = 2  /* Tier 2: Native JIT compiler (ARM64 / x86-64) */
} ObeliskTier;

/*
 * Virtual machine tape storage representation.
 * The tape consists of 8-bit unsigned cells with a dynamic or fixed bound,
 * tracking head position, high-water mark, and memory access telemetry.
 */
typedef struct {
    uint8_t *cells;
    size_t size;
    size_t head;
    size_t max_head_reached;
    bool is_dynamic;
} ObeliskTape;

/*
 * Virtual I/O callback hooks allowing the VM to stream directly into
 * network ring buffers, in-memory strings, or standard POSIX file descriptors.
 */
typedef int (*ObeliskReadFn)(void *user_data);
typedef int (*ObeliskWriteFn)(uint8_t byte, void *user_data);

/*
 * Execution statistics captured per run for telemetry and profiling.
 */
typedef struct {
    uint64_t total_instructions;
    uint64_t tape_reads;
    uint64_t tape_writes;
    uint64_t io_reads;
    uint64_t io_writes;
    uint64_t loop_iterations;
    uint64_t execution_time_ns;
} ObeliskTelemetry;

/*
 * Forward declaration of compiled bytecode program.
 */
typedef struct ObeliskProgram ObeliskProgram;

/*
 * Core virtual machine execution context.
 */
typedef struct {
    ObeliskTape tape;
    ObeliskReadFn read_fn;
    ObeliskWriteFn write_fn;
    void *io_user_data;
    uint64_t max_steps;
    uint64_t step_count;
    ObeliskTier tier;
    ObeliskTelemetry telemetry;
} ObeliskVM;

/*
 * Virtual Machine lifecycle management.
 */
ObeliskStatus obelisk_tape_init(ObeliskTape *tape, size_t initial_size, bool is_dynamic);
void obelisk_tape_free(ObeliskTape *tape);
void obelisk_tape_reset(ObeliskTape *tape);

ObeliskStatus obelisk_vm_init(ObeliskVM *vm, size_t tape_size, ObeliskTier tier);
void obelisk_vm_free(ObeliskVM *vm);
void obelisk_vm_reset(ObeliskVM *vm);
void obelisk_vm_set_io(ObeliskVM *vm, ObeliskReadFn read_fn, ObeliskWriteFn write_fn, void *user_data);

/*
 * Execution interfaces.
 */
ObeliskStatus obelisk_vm_run_source(ObeliskVM *vm, const char *source, size_t source_len);
ObeliskStatus obelisk_vm_run_program(ObeliskVM *vm, const ObeliskProgram *program);

#ifdef __cplusplus
}
#endif

#endif /* OBELISK_VM_H */
