#ifndef OBELISK_AOT_H
#define OBELISK_AOT_H

#include "obelisk/ir.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AOT configuration options.
 */
typedef struct {
    bool emit_bounds_checks;
    bool emit_telemetry;
    size_t tape_size;
    const char *function_name;
} ObeliskAotOptions;

/*
 * Ahead-of-time C source emission interfaces.
 */
bool obelisk_aot_transpile_to_c(const ObeliskProgram *program, const ObeliskAotOptions *options, FILE *out_file);
char *obelisk_aot_transpile_to_string(const ObeliskProgram *program, const ObeliskAotOptions *options);

#ifdef __cplusplus
}
#endif

#endif /* OBELISK_AOT_H */
