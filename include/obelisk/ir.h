#ifndef OBELISK_IR_H
#define OBELISK_IR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Intermediate Representation (IR) opcodes.
 * Lowered from Brainfuck characters into flattened, coalesced operations.
 */
typedef enum {
    IR_NOP = 0,         /* No operation */
    IR_ADD = 1,         /* Add immediate value to current cell: *ptr += val */
    IR_MOVE = 2,        /* Shift tape pointer by offset: ptr += val (negative for left) */
    IR_SET = 3,         /* Set current cell to fixed value: *ptr = val (e.g. [-] -> SET 0) */
    IR_READ = 4,        /* Read byte from input stream into cell: *ptr = getchar() */
    IR_WRITE = 5,       /* Write byte from cell to output stream: putchar(*ptr) */
    IR_JMP_IF_ZERO = 6, /* Jump to target if *ptr == 0 (lowered from '[') */
    IR_JMP_IF_NZ = 7,   /* Jump to target if *ptr != 0 (lowered from ']') */
    
    /* Advanced synthetic instructions produced by Tier 1 optimizer */
    IR_ADD_MULT = 8,    /* Multiply-add: ptr[offset] += *ptr * factor; *ptr = 0 */
    IR_SCAN_FWD = 9,    /* Scan forward for zero cell: while (*ptr) ptr += stride (SIMD/memchr) */
    IR_SCAN_BCK = 10    /* Scan backward for zero cell: while (*ptr) ptr -= stride */
} IrOpcode;

/*
 * Instruction structure within the linearized IR stream.
 */
typedef struct {
    IrOpcode opcode;
    int32_t val;        /* Arithmetic delta, pointer shift, or immediate literal */
    int32_t target;     /* Target instruction index for jumps */
    int32_t offset;     /* Secondary offset for multi-cell operations (e.g. IR_ADD_MULT) */
    int32_t factor;     /* Scaling factor for multiply-add operations */
} IrInstruction;

/*
 * Compiled program container holding linearized IR instructions,
 * metadata, and symbol table.
 */
struct ObeliskProgram {
    IrInstruction *instructions;
    size_t count;
    size_t capacity;
    size_t source_length;
    uint32_t optimization_level;
};

/*
 * IR Program lifecycle and manipulation functions.
 */
ObeliskProgram *obelisk_program_create(size_t initial_capacity);
void obelisk_program_free(ObeliskProgram *program);
bool obelisk_program_append(ObeliskProgram *program, IrInstruction instr);
void obelisk_program_dump(const ObeliskProgram *program);

#ifdef __cplusplus
}
#endif

#endif /* OBELISK_IR_H */
