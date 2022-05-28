#ifndef OBELISK_PROTOCOL_H
#define OBELISK_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Maximum command key and payload lengths.
 */
#define OBELISK_MAX_KEY_LEN      256
#define OBELISK_MAX_VALUE_LEN    8192
#define OBELISK_MAX_QUERY_LEN    16384

/*
 * Supported command types parsed from wire protocol streams.
 */
typedef enum {
    CMD_UNKNOWN = 0,
    CMD_PING = 1,
    CMD_SET = 2,
    CMD_GET = 3,
    CMD_DEL = 4,
    CMD_SCAN = 5,
    CMD_COUNT = 6,
    CMD_FILTER = 7,
    CMD_AGGREGATE = 8,
    CMD_TXN_BEGIN = 9,
    CMD_TXN_COMMIT = 10,
    CMD_TXN_ROLLBACK = 11,
    CMD_STATS = 12,
    CMD_COMPACT = 13,
    CMD_DUMP = 14
} ObeliskCommandType;

/*
 * Aggregation functions for analytics commands.
 */
typedef enum {
    AGG_NONE = 0,
    AGG_SUM = 1,
    AGG_AVG = 2,
    AGG_MIN = 3,
    AGG_MAX = 4,
    AGG_COUNT = 5
} ObeliskAggType;

/*
 * Parsed request command structure.
 */
typedef struct {
    ObeliskCommandType type;
    char key[OBELISK_MAX_KEY_LEN];
    char value[OBELISK_MAX_VALUE_LEN];
    char filter_field[OBELISK_MAX_KEY_LEN];
    char filter_op[8];
    char filter_val[OBELISK_MAX_KEY_LEN];
    ObeliskAggType agg_type;
    int32_t limit;
    int32_t offset;
} ObeliskCommand;

/*
 * Protocol parser and response encoder.
 */
bool obelisk_parse_command_line(const char *line, size_t len, ObeliskCommand *out_cmd);
const char *obelisk_command_name(ObeliskCommandType type);

#ifdef __cplusplus
}
#endif

#endif /* OBELISK_PROTOCOL_H */
