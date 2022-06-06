#include "obelisk/optimizer.h"
#include <stdlib.h>
#include <string.h>

/* Forward declaration from parser.c */
bool obelisk_parse_source(const char *source, size_t source_len, ObeliskProgram **out_program);

static void recompute_jumps(ObeliskProgram *prog) {
    size_t stack_cap = 256;
    size_t stack_size = 0;
    size_t *stack = (size_t *)malloc(sizeof(size_t) * stack_cap);
    if (!stack) return;

    for (size_t i = 0; i < prog->count; ++i) {
        if (prog->instructions[i].opcode == IR_JMP_IF_ZERO) {
            if (stack_size >= stack_cap) {
                stack_cap *= 2;
                size_t *new_s = (size_t *)realloc(stack, sizeof(size_t) * stack_cap);
                if (!new_s) { free(stack); return; }
                stack = new_s;
            }
            stack[stack_size++] = i;
        } else if (prog->instructions[i].opcode == IR_JMP_IF_NZ) {
            if (stack_size > 0) {
                size_t open_idx = stack[--stack_size];
                prog->instructions[i].target = (int32_t)open_idx + 1;
                prog->instructions[open_idx].target = (int32_t)i + 1;
            }
        }
    }
    free(stack);
}

bool obelisk_optimize_program(ObeliskProgram *program, uint32_t flags, ObeliskOptStats *stats) {
    if (!program || program->count == 0) {
        return false;
    }

    ObeliskOptStats local_stats;
    memset(&local_stats, 0, sizeof(local_stats));
    local_stats.instructions_before = program->count;

    ObeliskProgram *out = obelisk_program_create(program->count);
    if (!out) {
        return false;
    }

    size_t i = 0;
    while (i < program->count) {
        IrInstruction cur = program->instructions[i];

        /* Pass 1: Coalesce contiguous IR_ADD */
        if ((flags & OPT_PASS_COALESCE_ARITHMETIC) && cur.opcode == IR_ADD) {
            int32_t total = cur.val;
            size_t j = i + 1;
            while (j < program->count && program->instructions[j].opcode == IR_ADD) {
                total += program->instructions[j].val;
                j++;
            }
            local_stats.contiguous_ops_coalesced += (j - i - 1);
            i = j;
            cur.val = total;
            obelisk_program_append(out, cur);
            continue;
        }

        /* Pass 2: Coalesce contiguous IR_MOVE */
        if ((flags & OPT_PASS_COALESCE_SHIFTS) && cur.opcode == IR_MOVE) {
            int32_t total = cur.val;
            size_t j = i + 1;
            while (j < program->count && program->instructions[j].opcode == IR_MOVE) {
                total += program->instructions[j].val;
                j++;
            }
            local_stats.contiguous_ops_coalesced += (j - i - 1);
            i = j;
            if (total != 0) {
                cur.val = total;
                obelisk_program_append(out, cur);
            }
            continue;
        }

        /* Pass 3: Clear loops: [ - ] or [ + ] -> SET 0 */
        if ((flags & OPT_PASS_CLEAR_LOOPS) && cur.opcode == IR_JMP_IF_ZERO && (i + 2 < program->count)) {
            IrInstruction mid = program->instructions[i + 1];
            IrInstruction end = program->instructions[i + 2];
            if (mid.opcode == IR_ADD && (mid.val == -1 || mid.val == 1) && end.opcode == IR_JMP_IF_NZ) {
                IrInstruction set_op;
                memset(&set_op, 0, sizeof(set_op));
                set_op.opcode = IR_SET;
                set_op.val = 0;
                obelisk_program_append(out, set_op);
                local_stats.clear_loops_folded++;
                i += 3;
                continue;
            }
        }

        /* Pass 4: Zero scan loops: [ > ] -> SCAN_FWD, [ < ] -> SCAN_BCK */
        if ((flags & OPT_PASS_ZERO_SCANS) && cur.opcode == IR_JMP_IF_ZERO && (i + 2 < program->count)) {
            IrInstruction mid = program->instructions[i + 1];
            IrInstruction end = program->instructions[i + 2];
            if (mid.opcode == IR_MOVE && end.opcode == IR_JMP_IF_NZ) {
                IrInstruction scan_op;
                memset(&scan_op, 0, sizeof(scan_op));
                if (mid.val > 0) {
                    scan_op.opcode = IR_SCAN_FWD;
                    scan_op.val = mid.val;
                } else {
                    scan_op.opcode = IR_SCAN_BCK;
                    scan_op.val = -mid.val;
                }
                obelisk_program_append(out, scan_op);
                local_stats.zero_scans_folded++;
                i += 3;
                continue;
            }
        }

        /* Pass 5: Multiply-add loop: [ - > +++ < ] or [ > +++ < - ] */
        if ((flags & OPT_PASS_MULTIPLY_ADD) && cur.opcode == IR_JMP_IF_ZERO && (i + 4 < program->count)) {
            IrInstruction p1 = program->instructions[i + 1];
            IrInstruction p2 = program->instructions[i + 2];
            IrInstruction p3 = program->instructions[i + 3];
            IrInstruction p4 = program->instructions[i + 4];

            /* Pattern A: [ - > +k < ] */
            if (p1.opcode == IR_ADD && p1.val == -1 &&
                p2.opcode == IR_MOVE &&
                p3.opcode == IR_ADD &&
                p4.opcode == IR_MOVE && p4.val == -p2.val &&
                (i + 5 < program->count && program->instructions[i + 5].opcode == IR_JMP_IF_NZ)) {
                
                IrInstruction mad;
                memset(&mad, 0, sizeof(mad));
                mad.opcode = IR_ADD_MULT;
                mad.offset = p2.val;
                mad.factor = p3.val;
                obelisk_program_append(out, mad);
                local_stats.multiply_adds_folded++;
                i += 6;
                continue;
            }
        }

        obelisk_program_append(out, cur);
        i++;
    }

    recompute_jumps(out);

    free(program->instructions);
    program->instructions = out->instructions;
    program->count = out->count;
    program->capacity = out->capacity;
    program->optimization_level = flags;
    free(out);

    local_stats.instructions_after = program->count;
    if (stats) {
        *stats = local_stats;
    }
    return true;
}

bool obelisk_parse_and_optimize(const char *source, size_t source_len, uint32_t flags, ObeliskProgram **out_program) {
    if (!obelisk_parse_source(source, source_len, out_program)) {
        return false;
    }
    if (flags != 0) {
        obelisk_optimize_program(*out_program, flags, NULL);
    }
    return true;
}
