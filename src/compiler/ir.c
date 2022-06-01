#include "obelisk/ir.h"
#include <stdlib.h>
#include <stdio.h>

ObeliskProgram *obelisk_program_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 64;
    }
    ObeliskProgram *prog = (ObeliskProgram *)malloc(sizeof(ObeliskProgram));
    if (!prog) {
        return NULL;
    }
    prog->instructions = (IrInstruction *)malloc(sizeof(IrInstruction) * initial_capacity);
    if (!prog->instructions) {
        free(prog);
        return NULL;
    }
    prog->count = 0;
    prog->capacity = initial_capacity;
    prog->source_length = 0;
    prog->optimization_level = 0;
    return prog;
}

void obelisk_program_free(ObeliskProgram *program) {
    if (program) {
        if (program->instructions) {
            free(program->instructions);
            program->instructions = NULL;
        }
        free(program);
    }
}

bool obelisk_program_append(ObeliskProgram *program, IrInstruction instr) {
    if (!program) {
        return false;
    }
    if (program->count >= program->capacity) {
        size_t new_cap = program->capacity * 2;
        IrInstruction *new_inst = (IrInstruction *)realloc(program->instructions, sizeof(IrInstruction) * new_cap);
        if (!new_inst) {
            return false;
        }
        program->instructions = new_inst;
        program->capacity = new_cap;
    }
    program->instructions[program->count++] = instr;
    return true;
}

void obelisk_program_dump(const ObeliskProgram *program) {
    if (!program) return;
    printf("=== Obelisk Program IR Dump (Count: %zu) ===\n", program->count);
    for (size_t i = 0; i < program->count; ++i) {
        const IrInstruction *in = &program->instructions[i];
        switch (in->opcode) {
            case IR_NOP:
                printf("[%04zu] NOP\n", i);
                break;
            case IR_ADD:
                printf("[%04zu] ADD %+d\n", i, in->val);
                break;
            case IR_MOVE:
                printf("[%04zu] MOVE %+d\n", i, in->val);
                break;
            case IR_SET:
                printf("[%04zu] SET %d\n", i, in->val);
                break;
            case IR_READ:
                printf("[%04zu] READ\n", i);
                break;
            case IR_WRITE:
                printf("[%04zu] WRITE\n", i);
                break;
            case IR_JMP_IF_ZERO:
                printf("[%04zu] JZ -> %d\n", i, in->target);
                break;
            case IR_JMP_IF_NZ:
                printf("[%04zu] JNZ -> %d\n", i, in->target);
                break;
            case IR_ADD_MULT:
                printf("[%04zu] ADD_MULT offset: %+d, factor: %+d\n", i, in->offset, in->factor);
                break;
            case IR_SCAN_FWD:
                printf("[%04zu] SCAN_FWD stride: %d\n", i, in->val);
                break;
            case IR_SCAN_BCK:
                printf("[%04zu] SCAN_BCK stride: %d\n", i, in->val);
                break;
        }
    }
}
