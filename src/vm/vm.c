#include "obelisk/vm.h"
#include "obelisk/ir.h"
#include "obelisk/optimizer.h"
#include "obelisk/jit.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__GNUC__) || defined(__clang__)
#define OBELISK_HAS_COMPUTED_GOTO 1
#else
#define OBELISK_HAS_COMPUTED_GOTO 0
#endif

ObeliskStatus obelisk_vm_init(ObeliskVM *vm, size_t tape_size, ObeliskTier tier) {
    if (!vm) {
        return OBELISK_ERR_OUT_OF_MEMORY;
    }
    memset(vm, 0, sizeof(ObeliskVM));
    ObeliskStatus st = obelisk_tape_init(&vm->tape, tape_size, true);
    if (st != OBELISK_OK) {
        return st;
    }
    vm->tier = tier;
    vm->max_steps = OBELISK_DEFAULT_MAX_STEPS;
    return OBELISK_OK;
}

void obelisk_vm_free(ObeliskVM *vm) {
    if (vm) {
        obelisk_tape_free(&vm->tape);
        memset(vm, 0, sizeof(ObeliskVM));
    }
}

void obelisk_vm_reset(ObeliskVM *vm) {
    if (vm) {
        obelisk_tape_reset(&vm->tape);
        vm->step_count = 0;
        memset(&vm->telemetry, 0, sizeof(ObeliskTelemetry));
    }
}

void obelisk_vm_set_io(ObeliskVM *vm, ObeliskReadFn read_fn, ObeliskWriteFn write_fn, void *user_data) {
    if (vm) {
        vm->read_fn = read_fn;
        vm->write_fn = write_fn;
        vm->io_user_data = user_data;
    }
}

static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

ObeliskStatus obelisk_vm_run_program(ObeliskVM *vm, const ObeliskProgram *program) {
    if (!vm || !program || !program->instructions) {
        return OBELISK_ERR_INVALID_BYTECODE;
    }

    /* Route to Tier 2 native JIT compiler if selected and target is supported */
    if (vm->tier == OBELISK_TIER_JIT && obelisk_jit_detect_host_target() != JIT_TARGET_NONE) {
        ObeliskJitBuffer jit_buf;
        memset(&jit_buf, 0, sizeof(jit_buf));
        ObeliskStatus jit_st = obelisk_jit_compile(program, &jit_buf);
        if (jit_st == OBELISK_OK) {
            uint64_t start_time = get_time_ns();
            ObeliskStatus exec_st = obelisk_jit_execute(&jit_buf, vm);
            vm->telemetry.execution_time_ns = get_time_ns() - start_time;
            obelisk_jit_buffer_free(&jit_buf);
            return exec_st;
        }
        /* Fall back to bytecode interpreter if JIT compilation fails */
    }

    uint8_t *cells = vm->tape.cells;
    size_t tape_size = vm->tape.size;
    size_t head = vm->tape.head;
    size_t pc = 0;
    size_t count = program->count;
    const IrInstruction *code = program->instructions;
    uint64_t steps = 0;
    uint64_t max_steps = vm->max_steps;
    uint64_t start_time = get_time_ns();

#if OBELISK_HAS_COMPUTED_GOTO
    static const void *dispatch_table[] = {
        &&do_nop,
        &&do_add,
        &&do_move,
        &&do_set,
        &&do_read,
        &&do_write,
        &&do_jmp_zero,
        &&do_jmp_nz,
        &&do_add_mult,
        &&do_scan_fwd,
        &&do_scan_bck
    };

    #define DISPATCH() do { \
        if (pc >= count) goto execution_done; \
        if (steps++ >= max_steps) { \
            vm->tape.head = head; \
            vm->step_count = steps; \
            vm->telemetry.execution_time_ns = get_time_ns() - start_time; \
            return OBELISK_ERR_STEP_LIMIT_EXCEEDED; \
        } \
        goto *dispatch_table[code[pc].opcode]; \
    } while (0)

    DISPATCH();

do_nop:
    pc++;
    DISPATCH();

do_add:
    cells[head] = (uint8_t)(cells[head] + code[pc].val);
    vm->telemetry.tape_writes++;
    pc++;
    DISPATCH();

do_move:
    {
        int64_t new_head = (int64_t)head + code[pc].val;
        if (new_head < 0 || (size_t)new_head >= tape_size) {
            vm->tape.head = head;
            vm->step_count = steps;
            vm->telemetry.execution_time_ns = get_time_ns() - start_time;
            return OBELISK_ERR_OUT_OF_BOUNDS;
        }
        head = (size_t)new_head;
        if (head > vm->tape.max_head_reached) {
            vm->tape.max_head_reached = head;
        }
    }
    pc++;
    DISPATCH();

do_set:
    cells[head] = (uint8_t)code[pc].val;
    vm->telemetry.tape_writes++;
    pc++;
    DISPATCH();

do_read:
    if (vm->read_fn) {
        int b = vm->read_fn(vm->io_user_data);
        if (b >= 0) {
            cells[head] = (uint8_t)b;
            vm->telemetry.io_reads++;
            vm->telemetry.tape_writes++;
        }
    }
    pc++;
    DISPATCH();

do_write:
    if (vm->write_fn) {
        if (vm->write_fn(cells[head], vm->io_user_data) != 0) {
            vm->tape.head = head;
            vm->step_count = steps;
            vm->telemetry.execution_time_ns = get_time_ns() - start_time;
            return OBELISK_ERR_IO_WRITE;
        }
        vm->telemetry.io_writes++;
        vm->telemetry.tape_reads++;
    }
    pc++;
    DISPATCH();

do_jmp_zero:
    if (cells[head] == 0) {
        pc = (size_t)code[pc].target;
    } else {
        pc++;
    }
    vm->telemetry.loop_iterations++;
    DISPATCH();

do_jmp_nz:
    if (cells[head] != 0) {
        pc = (size_t)code[pc].target;
    } else {
        pc++;
    }
    vm->telemetry.loop_iterations++;
    DISPATCH();

do_add_mult:
    {
        uint8_t cur = cells[head];
        if (cur != 0) {
            int64_t target_idx = (int64_t)head + code[pc].offset;
            if (target_idx < 0 || (size_t)target_idx >= tape_size) {
                vm->tape.head = head;
                vm->step_count = steps;
                return OBELISK_ERR_OUT_OF_BOUNDS;
            }
            cells[target_idx] = (uint8_t)(cells[target_idx] + cur * code[pc].factor);
            cells[head] = 0;
            vm->telemetry.tape_writes += 2;
            vm->telemetry.tape_reads++;
        }
    }
    pc++;
    DISPATCH();

do_scan_fwd:
    {
        int32_t stride = code[pc].val ? code[pc].val : 1;
        if (stride == 1) {
            /* Acceleration using memchr */
            size_t remaining = tape_size - head;
            void *zero_ptr = memchr(cells + head, 0, remaining);
            if (zero_ptr) {
                head = (size_t)((uint8_t *)zero_ptr - cells);
            } else {
                vm->tape.head = head;
                vm->step_count = steps;
                return OBELISK_ERR_OUT_OF_BOUNDS;
            }
        } else {
            while (head < tape_size && cells[head] != 0) {
                head += (size_t)stride;
            }
            if (head >= tape_size) {
                vm->tape.head = head;
                vm->step_count = steps;
                return OBELISK_ERR_OUT_OF_BOUNDS;
            }
        }
        if (head > vm->tape.max_head_reached) {
            vm->tape.max_head_reached = head;
        }
    }
    pc++;
    DISPATCH();

do_scan_bck:
    {
        int32_t stride = code[pc].val ? code[pc].val : 1;
        while (head > 0 && cells[head] != 0) {
            if (head < (size_t)stride) {
                head = 0;
                break;
            }
            head -= (size_t)stride;
        }
        if (cells[head] != 0) {
            vm->tape.head = head;
            vm->step_count = steps;
            return OBELISK_ERR_OUT_OF_BOUNDS;
        }
    }
    pc++;
    DISPATCH();

#else
    /* Fallback standard switch dispatch */
    while (pc < count) {
        if (steps++ >= max_steps) {
            vm->tape.head = head;
            vm->step_count = steps;
            vm->telemetry.execution_time_ns = get_time_ns() - start_time;
            return OBELISK_ERR_STEP_LIMIT_EXCEEDED;
        }

        const IrInstruction *instr = &code[pc];
        switch (instr->opcode) {
            case IR_NOP:
                pc++;
                break;
            case IR_ADD:
                cells[head] = (uint8_t)(cells[head] + instr->val);
                vm->telemetry.tape_writes++;
                pc++;
                break;
            case IR_MOVE: {
                int64_t new_head = (int64_t)head + instr->val;
                if (new_head < 0 || (size_t)new_head >= tape_size) {
                    vm->tape.head = head;
                    vm->step_count = steps;
                    vm->telemetry.execution_time_ns = get_time_ns() - start_time;
                    return OBELISK_ERR_OUT_OF_BOUNDS;
                }
                head = (size_t)new_head;
                if (head > vm->tape.max_head_reached) {
                    vm->tape.max_head_reached = head;
                }
                pc++;
                break;
            }
            case IR_SET:
                cells[head] = (uint8_t)instr->val;
                vm->telemetry.tape_writes++;
                pc++;
                break;
            case IR_READ:
                if (vm->read_fn) {
                    int b = vm->read_fn(vm->io_user_data);
                    if (b >= 0) {
                        cells[head] = (uint8_t)b;
                        vm->telemetry.io_reads++;
                        vm->telemetry.tape_writes++;
                    }
                }
                pc++;
                break;
            case IR_WRITE:
                if (vm->write_fn) {
                    if (vm->write_fn(cells[head], vm->io_user_data) != 0) {
                        vm->tape.head = head;
                        vm->step_count = steps;
                        vm->telemetry.execution_time_ns = get_time_ns() - start_time;
                        return OBELISK_ERR_IO_WRITE;
                    }
                    vm->telemetry.io_writes++;
                    vm->telemetry.tape_reads++;
                }
                pc++;
                break;
            case IR_JMP_IF_ZERO:
                if (cells[head] == 0) {
                    pc = (size_t)instr->target;
                } else {
                    pc++;
                }
                vm->telemetry.loop_iterations++;
                break;
            case IR_JMP_IF_NZ:
                if (cells[head] != 0) {
                    pc = (size_t)instr->target;
                } else {
                    pc++;
                }
                vm->telemetry.loop_iterations++;
                break;
            case IR_ADD_MULT: {
                uint8_t cur = cells[head];
                if (cur != 0) {
                    int64_t target_idx = (int64_t)head + instr->offset;
                    if (target_idx < 0 || (size_t)target_idx >= tape_size) {
                        vm->tape.head = head;
                        vm->step_count = steps;
                        return OBELISK_ERR_OUT_OF_BOUNDS;
                    }
                    cells[target_idx] = (uint8_t)(cells[target_idx] + cur * instr->factor);
                    cells[head] = 0;
                    vm->telemetry.tape_writes += 2;
                    vm->telemetry.tape_reads++;
                }
                pc++;
                break;
            }
            case IR_SCAN_FWD: {
                int32_t stride = instr->val ? instr->val : 1;
                while (head < tape_size && cells[head] != 0) {
                    head += (size_t)stride;
                }
                if (head >= tape_size) {
                    vm->tape.head = head;
                    vm->step_count = steps;
                    return OBELISK_ERR_OUT_OF_BOUNDS;
                }
                if (head > vm->tape.max_head_reached) {
                    vm->tape.max_head_reached = head;
                }
                pc++;
                break;
            }
            case IR_SCAN_BCK: {
                int32_t stride = instr->val ? instr->val : 1;
                while (head > 0 && cells[head] != 0) {
                    if (head < (size_t)stride) {
                        head = 0;
                        break;
                    }
                    head -= (size_t)stride;
                }
                pc++;
                break;
            }
            default:
                pc++;
                break;
        }
    }
#endif

execution_done:
    vm->tape.head = head;
    vm->step_count = steps;
    vm->telemetry.total_instructions = steps;
    vm->telemetry.execution_time_ns = get_time_ns() - start_time;
    return OBELISK_OK;
}

ObeliskStatus obelisk_vm_run_source(ObeliskVM *vm, const char *source, size_t source_len) {
    if (!vm || !source) {
        return OBELISK_ERR_INVALID_BYTECODE;
    }

    uint32_t opt_flags = (vm->tier == OBELISK_TIER_INTERPRETER) ? 0 : OPT_PASS_ALL;
    ObeliskProgram *prog = NULL;
    if (!obelisk_parse_and_optimize(source, source_len, opt_flags, &prog)) {
        return OBELISK_ERR_UNMATCHED_BRACKET;
    }

    ObeliskStatus st = obelisk_vm_run_program(vm, prog);
    obelisk_program_free(prog);
    return st;
}
