#include "obelisk/ir.h"
#include <stdlib.h>
#include <string.h>

bool obelisk_parse_source(const char *source, size_t source_len, ObeliskProgram **out_program) {
    if (!source || !out_program) {
        return false;
    }
    if (source_len == 0) {
        source_len = strlen(source);
    }

    ObeliskProgram *prog = obelisk_program_create(source_len > 16 ? source_len : 16);
    if (!prog) {
        return false;
    }
    prog->source_length = source_len;

    /* Bracket stack for pairing '[' and ']' */
    size_t stack_cap = 256;
    size_t stack_size = 0;
    size_t *stack = (size_t *)malloc(sizeof(size_t) * stack_cap);
    if (!stack) {
        obelisk_program_free(prog);
        return false;
    }

    for (size_t i = 0; i < source_len; ++i) {
        char ch = source[i];
        IrInstruction instr;
        memset(&instr, 0, sizeof(IrInstruction));

        switch (ch) {
            case '+':
                instr.opcode = IR_ADD;
                instr.val = 1;
                obelisk_program_append(prog, instr);
                break;
            case '-':
                instr.opcode = IR_ADD;
                instr.val = -1;
                obelisk_program_append(prog, instr);
                break;
            case '>':
                instr.opcode = IR_MOVE;
                instr.val = 1;
                obelisk_program_append(prog, instr);
                break;
            case '<':
                instr.opcode = IR_MOVE;
                instr.val = -1;
                obelisk_program_append(prog, instr);
                break;
            case '.':
                instr.opcode = IR_WRITE;
                obelisk_program_append(prog, instr);
                break;
            case ',':
                instr.opcode = IR_READ;
                obelisk_program_append(prog, instr);
                break;
            case '[':
                if (stack_size >= stack_cap) {
                    stack_cap *= 2;
                    size_t *new_stack = (size_t *)realloc(stack, sizeof(size_t) * stack_cap);
                    if (!new_stack) {
                        free(stack);
                        obelisk_program_free(prog);
                        return false;
                    }
                    stack = new_stack;
                }
                stack[stack_size++] = prog->count;
                instr.opcode = IR_JMP_IF_ZERO;
                instr.target = -1; /* Resolved upon encountering matching ']' */
                obelisk_program_append(prog, instr);
                break;
            case ']':
                if (stack_size == 0) {
                    /* Unmatched closing bracket */
                    free(stack);
                    obelisk_program_free(prog);
                    return false;
                }
                {
                    size_t open_idx = stack[--stack_size];
                    instr.opcode = IR_JMP_IF_NZ;
                    instr.target = (int32_t)open_idx + 1;
                    prog->instructions[open_idx].target = (int32_t)prog->count + 1;
                    obelisk_program_append(prog, instr);
                }
                break;
            default:
                /* Non-command characters are treated as comments and ignored */
                break;
        }
    }

    if (stack_size != 0) {
        /* Unmatched opening bracket */
        free(stack);
        obelisk_program_free(prog);
        return false;
    }

    free(stack);
    *out_program = prog;
    return true;
}
