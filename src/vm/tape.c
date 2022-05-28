#include "obelisk/vm.h"
#include <stdlib.h>
#include <string.h>

ObeliskStatus obelisk_tape_init(ObeliskTape *tape, size_t initial_size, bool is_dynamic) {
    if (!tape) {
        return OBELISK_ERR_OUT_OF_MEMORY;
    }
    if (initial_size == 0) {
        initial_size = OBELISK_DEFAULT_TAPE_SIZE;
    }
    if (initial_size > OBELISK_MAX_TAPE_SIZE) {
        initial_size = OBELISK_MAX_TAPE_SIZE;
    }

    tape->cells = (uint8_t *)calloc(initial_size, sizeof(uint8_t));
    if (!tape->cells) {
        return OBELISK_ERR_OUT_OF_MEMORY;
    }

    tape->size = initial_size;
    tape->head = 0;
    tape->max_head_reached = 0;
    tape->is_dynamic = is_dynamic;
    return OBELISK_OK;
}

void obelisk_tape_free(ObeliskTape *tape) {
    if (tape && tape->cells) {
        free(tape->cells);
        tape->cells = NULL;
        tape->size = 0;
        tape->head = 0;
        tape->max_head_reached = 0;
        tape->is_dynamic = false;
    }
}

void obelisk_tape_reset(ObeliskTape *tape) {
    if (tape && tape->cells) {
        memset(tape->cells, 0, tape->size);
        tape->head = 0;
        tape->max_head_reached = 0;
    }
}
