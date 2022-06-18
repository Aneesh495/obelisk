#include "obelisk/jit.h"
#include <stdlib.h>
#include <string.h>

#define EMIT_BYTE(b) do { \
    if (len + 1 > max_size) return false; \
    code[len++] = (uint8_t)(b); \
} while (0)

#define EMIT_INT32(val) do { \
    if (len + 4 > max_size) return false; \
    *(int32_t *)(code + len) = (int32_t)(val); \
    len += 4; \
} while (0)

bool obelisk_jit_emit_x86_64(const ObeliskProgram *prog, uint8_t *code, size_t max_size, size_t *out_size) {
    if (!prog || !code || max_size < 256) {
        return false;
    }

    size_t len = 0;

    /* Function Prologue:
     * push rbp
     * mov rbp, rsp
     * push r12
     * push r13
     * mov r12, rdi   (r12 = tape pointer)
     * mov r13, rsi   (r13 = io_ctx / vm)
     */
    EMIT_BYTE(0x55);             /* push rbp */
    EMIT_BYTE(0x48); EMIT_BYTE(0x89); EMIT_BYTE(0xe5); /* mov rbp, rsp */
    EMIT_BYTE(0x41); EMIT_BYTE(0x54); /* push r12 */
    EMIT_BYTE(0x41); EMIT_BYTE(0x55); /* push r13 */
    EMIT_BYTE(0x49); EMIT_BYTE(0x89); EMIT_BYTE(0xfc); /* mov r12, rdi */
    EMIT_BYTE(0x49); EMIT_BYTE(0x89); EMIT_BYTE(0xf5); /* mov r13, rsi */

    size_t *instr_offsets = (size_t *)malloc(sizeof(size_t) * (prog->count + 1));
    if (!instr_offsets) return false;

    typedef struct {
        size_t code_offset;
        size_t target_instr;
        bool is_jz;
    } JumpFixup;

    size_t fixup_cap = 64;
    size_t fixup_count = 0;
    JumpFixup *fixups = (JumpFixup *)malloc(sizeof(JumpFixup) * fixup_cap);
    if (!fixups) { free(instr_offsets); return false; }

    for (size_t i = 0; i < prog->count; ++i) {
        instr_offsets[i] = len;
        const IrInstruction *in = &prog->instructions[i];

        switch (in->opcode) {
            case IR_NOP:
                EMIT_BYTE(0x90); /* nop */
                break;

            case IR_ADD: {
                int32_t v = (in->val % 256 + 256) % 256;
                if (v != 0) {
                    /* add byte [r12], v */
                    EMIT_BYTE(0x41); EMIT_BYTE(0x80); EMIT_BYTE(0x04); EMIT_BYTE(0x24);
                    EMIT_BYTE((uint8_t)v);
                }
                break;
            }

            case IR_MOVE: {
                int32_t v = in->val;
                if (v > 0) {
                    /* add r12, imm32 */
                    EMIT_BYTE(0x49); EMIT_BYTE(0x81); EMIT_BYTE(0xc4);
                    EMIT_INT32(v);
                } else if (v < 0) {
                    /* sub r12, imm32 */
                    EMIT_BYTE(0x49); EMIT_BYTE(0x81); EMIT_BYTE(0xec);
                    EMIT_INT32(-v);
                }
                break;
            }

            case IR_SET: {
                /* mov byte [r12], imm8 */
                EMIT_BYTE(0x41); EMIT_BYTE(0xc6); EMIT_BYTE(0x04); EMIT_BYTE(0x24);
                EMIT_BYTE((uint8_t)(in->val & 0xff));
                break;
            }

            case IR_JMP_IF_ZERO: {
                /* cmp byte [r12], 0 */
                EMIT_BYTE(0x41); EMIT_BYTE(0x80); EMIT_BYTE(0x3c); EMIT_BYTE(0x24); EMIT_BYTE(0x00);
                /* jz rel32 (0x0f 0x84 rel32) */
                EMIT_BYTE(0x0f); EMIT_BYTE(0x84);
                if (fixup_count >= fixup_cap) {
                    fixup_cap *= 2;
                    JumpFixup *nf = (JumpFixup *)realloc(fixups, sizeof(JumpFixup) * fixup_cap);
                    if (!nf) { free(instr_offsets); free(fixups); return false; }
                    fixups = nf;
                }
                fixups[fixup_count].code_offset = len;
                fixups[fixup_count].target_instr = (size_t)in->target;
                fixups[fixup_count].is_jz = true;
                fixup_count++;
                EMIT_INT32(0); /* Placeholder */
                break;
            }

            case IR_JMP_IF_NZ: {
                /* cmp byte [r12], 0 */
                EMIT_BYTE(0x41); EMIT_BYTE(0x80); EMIT_BYTE(0x3c); EMIT_BYTE(0x24); EMIT_BYTE(0x00);
                /* jnz rel32 (0x0f 0x85 rel32) */
                EMIT_BYTE(0x0f); EMIT_BYTE(0x85);
                if (fixup_count >= fixup_cap) {
                    fixup_cap *= 2;
                    JumpFixup *nf = (JumpFixup *)realloc(fixups, sizeof(JumpFixup) * fixup_cap);
                    if (!nf) { free(instr_offsets); free(fixups); return false; }
                    fixups = nf;
                }
                fixups[fixup_count].code_offset = len;
                fixups[fixup_count].target_instr = (size_t)in->target;
                fixups[fixup_count].is_jz = false;
                fixup_count++;
                EMIT_INT32(0); /* Placeholder */
                break;
            }

            default:
                EMIT_BYTE(0x90);
                break;
        }
    }

    instr_offsets[prog->count] = len;

    /* Epilogue:
     * mov rax, r12
     * pop r13
     * pop r12
     * pop rbp
     * ret
     */
    EMIT_BYTE(0x4c); EMIT_BYTE(0x89); EMIT_BYTE(0xe0); /* mov rax, r12 */
    EMIT_BYTE(0x41); EMIT_BYTE(0x5d);                   /* pop r13 */
    EMIT_BYTE(0x41); EMIT_BYTE(0x5c);                   /* pop r12 */
    EMIT_BYTE(0x5d);                                     /* pop rbp */
    EMIT_BYTE(0xc3);                                     /* ret */

    /* Backpatch jumps */
    for (size_t f = 0; f < fixup_count; ++f) {
        size_t target_idx = fixups[f].target_instr;
        if (target_idx > prog->count) target_idx = prog->count;
        size_t target_code_offset = instr_offsets[target_idx];
        int64_t diff = (int64_t)target_code_offset - ((int64_t)fixups[f].code_offset + 4);
        *(int32_t *)(code + fixups[f].code_offset) = (int32_t)diff;
    }

    free(instr_offsets);
    free(fixups);

    *out_size = len;
    return true;
}
