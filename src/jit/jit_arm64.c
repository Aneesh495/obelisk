#include "obelisk/jit.h"
#include <stdlib.h>
#include <string.h>

#define EMIT32(val) do { \
    if (len + 4 > max_size) return false; \
    *(uint32_t *)(code + len) = (uint32_t)(val); \
    len += 4; \
} while (0)

bool obelisk_jit_emit_arm64(const ObeliskProgram *prog, uint8_t *code, size_t max_size, size_t *out_size) {
    if (!prog || !code || max_size < 256) {
        return false;
    }

    size_t len = 0;

    /* Function Prologue:
     * stp x29, x30, [sp, #-32]!
     * stp x19, x20, [sp, #16]
     * mov x29, sp
     * mov x19, x0   (x19 = tape pointer)
     * mov x20, x1   (x20 = io_ctx / vm pointer)
     */
    EMIT32(0xa9be7bfd); /* stp x29, x30, [sp, #-32]! */
    EMIT32(0xa90153f3); /* stp x19, x20, [sp, #16] */
    EMIT32(0x910003fd); /* mov x29, sp */
    EMIT32(0xaa0003f3); /* mov x19, x0 */
    EMIT32(0xaa0103f4); /* mov x20, x1 */

    size_t *instr_offsets = (size_t *)malloc(sizeof(size_t) * (prog->count + 1));
    if (!instr_offsets) return false;

    typedef struct {
        size_t code_offset;
        size_t target_instr;
        bool is_cbz;
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
                EMIT32(0xd503201f); /* nop */
                break;

            case IR_ADD: {
                int32_t v = (in->val % 256 + 256) % 256;
                if (v != 0) {
                    EMIT32(0x39400268); /* ldrb w8, [x19] */
                    if (v > 0) {
                        EMIT32(0x11000108 | ((uint32_t)(v & 0xfff) << 10)); /* add w8, w8, #imm */
                    }
                    EMIT32(0x39000268); /* strb w8, [x19] */
                }
                break;
            }

            case IR_MOVE: {
                int32_t v = in->val;
                if (v > 0) {
                    uint32_t imm = (uint32_t)v;
                    while (imm > 4095) {
                        EMIT32(0x91000273 | (4095 << 10)); /* add x19, x19, #4095 */
                        imm -= 4095;
                    }
                    if (imm > 0) {
                        EMIT32(0x91000273 | (imm << 10)); /* add x19, x19, #imm */
                    }
                } else if (v < 0) {
                    uint32_t imm = (uint32_t)(-v);
                    while (imm > 4095) {
                        EMIT32(0xd1000273 | (4095 << 10)); /* sub x19, x19, #4095 */
                        imm -= 4095;
                    }
                    if (imm > 0) {
                        EMIT32(0xd1000273 | (imm << 10)); /* sub x19, x19, #imm */
                    }
                }
                break;
            }

            case IR_SET: {
                uint32_t val = (uint32_t)(in->val & 0xff);
                EMIT32(0x52800008 | (val << 5)); /* mov w8, #imm */
                EMIT32(0x39000268);               /* strb w8, [x19] */
                break;
            }

            case IR_JMP_IF_ZERO: {
                EMIT32(0x39400268); /* ldrb w8, [x19] */
                if (fixup_count >= fixup_cap) {
                    fixup_cap *= 2;
                    JumpFixup *nf = (JumpFixup *)realloc(fixups, sizeof(JumpFixup) * fixup_cap);
                    if (!nf) { free(instr_offsets); free(fixups); return false; }
                    fixups = nf;
                }
                fixups[fixup_count].code_offset = len;
                fixups[fixup_count].target_instr = (size_t)in->target;
                fixups[fixup_count].is_cbz = true;
                fixup_count++;
                EMIT32(0x34000008); /* placeholder: cbz w8, #0 */
                break;
            }

            case IR_JMP_IF_NZ: {
                EMIT32(0x39400268); /* ldrb w8, [x19] */
                if (fixup_count >= fixup_cap) {
                    fixup_cap *= 2;
                    JumpFixup *nf = (JumpFixup *)realloc(fixups, sizeof(JumpFixup) * fixup_cap);
                    if (!nf) { free(instr_offsets); free(fixups); return false; }
                    fixups = nf;
                }
                fixups[fixup_count].code_offset = len;
                fixups[fixup_count].target_instr = (size_t)in->target;
                fixups[fixup_count].is_cbz = false;
                fixup_count++;
                EMIT32(0x35000008); /* placeholder: cbnz w8, #0 */
                break;
            }

            default:
                /* NOP for other complex ops in simple JIT mode */
                EMIT32(0xd503201f);
                break;
        }
    }

    instr_offsets[prog->count] = len;

    /* Epilogue */
    EMIT32(0xaa1303e0); /* mov x0, x19 (return final tape pointer) */
    EMIT32(0xa94153f3); /* ldp x19, x20, [sp, #16] */
    EMIT32(0xa8c27bfd); /* ldp x29, x30, [sp], #32 */
    EMIT32(0xd65f03c0); /* ret */

    /* Backpatch jumps */
    for (size_t f = 0; f < fixup_count; ++f) {
        size_t target_idx = fixups[f].target_instr;
        if (target_idx > prog->count) target_idx = prog->count;
        size_t target_code_offset = instr_offsets[target_idx];
        int64_t diff_bytes = (int64_t)target_code_offset - (int64_t)fixups[f].code_offset;
        int32_t imm19 = (int32_t)(diff_bytes / 4);

        uint32_t opcode = fixups[f].is_cbz ? 0x34000008 : 0x35000008;
        uint32_t patched = opcode | ((uint32_t)(imm19 & 0x7ffff) << 5);
        *(uint32_t *)(code + fixups[f].code_offset) = patched;
    }

    free(instr_offsets);
    free(fixups);

    *out_size = len;
    return true;
}
