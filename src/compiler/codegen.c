#include "obelisk/ir.h"
#include <stdlib.h>
#include <string.h>

char *obelisk_ir_to_brainfuck(const ObeliskProgram *program) {
    if (!program || program->count == 0) {
        char *empty = (char *)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    size_t cap = program->count * 8 + 64;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    #define APPEND_CHAR(c) do { \
        if (len + 2 >= cap) { \
            cap *= 2; \
            char *nb = (char *)realloc(buf, cap); \
            if (!nb) { free(buf); return NULL; } \
            buf = nb; \
        } \
        buf[len++] = (c); \
    } while (0)

    for (size_t i = 0; i < program->count; ++i) {
        const IrInstruction *in = &program->instructions[i];
        switch (in->opcode) {
            case IR_ADD:
                if (in->val > 0) {
                    for (int32_t k = 0; k < in->val; ++k) APPEND_CHAR('+');
                } else if (in->val < 0) {
                    for (int32_t k = 0; k < -in->val; ++k) APPEND_CHAR('-');
                }
                break;
            case IR_MOVE:
                if (in->val > 0) {
                    for (int32_t k = 0; k < in->val; ++k) APPEND_CHAR('>');
                } else if (in->val < 0) {
                    for (int32_t k = 0; k < -in->val; ++k) APPEND_CHAR('<');
                }
                break;
            case IR_SET:
                APPEND_CHAR('[');
                APPEND_CHAR('-');
                APPEND_CHAR(']');
                if (in->val > 0) {
                    for (int32_t k = 0; k < in->val; ++k) APPEND_CHAR('+');
                }
                break;
            case IR_READ:
                APPEND_CHAR(',');
                break;
            case IR_WRITE:
                APPEND_CHAR('.');
                break;
            case IR_JMP_IF_ZERO:
                APPEND_CHAR('[');
                break;
            case IR_JMP_IF_NZ:
                APPEND_CHAR(']');
                break;
            case IR_ADD_MULT:
                APPEND_CHAR('[');
                APPEND_CHAR('-');
                if (in->offset > 0) {
                    for (int32_t k = 0; k < in->offset; ++k) APPEND_CHAR('>');
                } else {
                    for (int32_t k = 0; k < -in->offset; ++k) APPEND_CHAR('<');
                }
                if (in->factor > 0) {
                    for (int32_t k = 0; k < in->factor; ++k) APPEND_CHAR('+');
                } else {
                    for (int32_t k = 0; k < -in->factor; ++k) APPEND_CHAR('-');
                }
                if (in->offset > 0) {
                    for (int32_t k = 0; k < in->offset; ++k) APPEND_CHAR('<');
                } else {
                    for (int32_t k = 0; k < -in->offset; ++k) APPEND_CHAR('>');
                }
                APPEND_CHAR(']');
                break;
            case IR_SCAN_FWD:
                APPEND_CHAR('[');
                for (int32_t k = 0; k < (in->val ? in->val : 1); ++k) APPEND_CHAR('>');
                APPEND_CHAR(']');
                break;
            case IR_SCAN_BCK:
                APPEND_CHAR('[');
                for (int32_t k = 0; k < (in->val ? in->val : 1); ++k) APPEND_CHAR('<');
                APPEND_CHAR(']');
                break;
            default:
                break;
        }
    }

    buf[len] = '\0';
    return buf;
}
