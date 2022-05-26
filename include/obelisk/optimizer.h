#ifndef OBELISK_OPTIMIZER_H
#define OBELISK_OPTIMIZER_H

#include "obelisk/ir.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optimization pass configuration flags.
 */
typedef enum {
    OPT_PASS_COALESCE_ARITHMETIC = (1 << 0), /* Coalesce contiguous '+' and '-' into single ADD */
    OPT_PASS_COALESCE_SHIFTS     = (1 << 1), /* Coalesce contiguous '>' and '<' into single MOVE */
    OPT_PASS_CLEAR_LOOPS         = (1 << 2), /* Recognize '[-]' and convert to SET 0 */
    OPT_PASS_MULTIPLY_ADD        = (1 << 3), /* Recognize '[->+++<]' and lower to ADD_MULT */
    OPT_PASS_ZERO_SCANS          = (1 << 4), /* Recognize '[>]' / '[<]' and lower to SCAN_FWD / SCAN_BCK */
    OPT_PASS_DEAD_CODE           = (1 << 5), /* Eliminate dead stores and unreachable loop blocks */
    OPT_PASS_ALL                 = 0x3F
} ObeliskOptFlags;

/*
 * Optimization telemetry capturing transformations performed.
 */
typedef struct {
    size_t instructions_before;
    size_t instructions_after;
    size_t clear_loops_folded;
    size_t multiply_adds_folded;
    size_t zero_scans_folded;
    size_t contiguous_ops_coalesced;
    size_t dead_instructions_eliminated;
} ObeliskOptStats;

/*
 * Optimization pipeline entrypoints.
 */
bool obelisk_optimize_program(ObeliskProgram *program, uint32_t flags, ObeliskOptStats *stats);
bool obelisk_parse_and_optimize(const char *source, size_t source_len, uint32_t flags, ObeliskProgram **out_program);

#ifdef __cplusplus
}
#endif

#endif /* OBELISK_OPTIMIZER_H */
