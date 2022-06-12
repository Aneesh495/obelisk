#include "obelisk/jit.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#endif

ObeliskJitTarget obelisk_jit_detect_host_target(void) {
#if defined(OBELISK_TARGET_ARM64) || defined(__aarch64__) || defined(__arm64__)
    return JIT_TARGET_ARM64;
#elif defined(OBELISK_TARGET_X86_64) || defined(__x86_64__) || defined(_M_X64)
    return JIT_TARGET_X86_64;
#else
    return JIT_TARGET_NONE;
#endif
}

/* Forward declarations for architecture-specific emitters */
bool obelisk_jit_emit_arm64(const ObeliskProgram *prog, uint8_t *code, size_t max_size, size_t *out_size);
bool obelisk_jit_emit_x86_64(const ObeliskProgram *prog, uint8_t *code, size_t max_size, size_t *out_size);

ObeliskStatus obelisk_jit_compile(const ObeliskProgram *program, ObeliskJitBuffer *out_buffer) {
    if (!program || !out_buffer) {
        return OBELISK_ERR_JIT_COMPILATION;
    }

    ObeliskJitTarget target = obelisk_jit_detect_host_target();
    if (target == JIT_TARGET_NONE) {
        return OBELISK_ERR_JIT_COMPILATION;
    }

    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t estimated_code_size = program->count * 32 + 256;
    size_t alloc_size = ((estimated_code_size + page_size - 1) / page_size) * page_size;

    void *mem = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) {
        return OBELISK_ERR_OUT_OF_MEMORY;
    }

    size_t code_bytes = 0;
    bool emit_ok = false;

    if (target == JIT_TARGET_ARM64) {
        emit_ok = obelisk_jit_emit_arm64(program, (uint8_t *)mem, alloc_size, &code_bytes);
    } else if (target == JIT_TARGET_X86_64) {
        emit_ok = obelisk_jit_emit_x86_64(program, (uint8_t *)mem, alloc_size, &code_bytes);
    }

    if (!emit_ok) {
        munmap(mem, alloc_size);
        return OBELISK_ERR_JIT_COMPILATION;
    }

#if defined(__APPLE__)
    sys_dcache_flush(mem, code_bytes);
#endif

    if (mprotect(mem, alloc_size, PROT_READ | PROT_EXEC) != 0) {
        munmap(mem, alloc_size);
        return OBELISK_ERR_JIT_COMPILATION;
    }

#if defined(__APPLE__)
    sys_icache_invalidate(mem, code_bytes);
#endif

    out_buffer->executable_memory = mem;
    out_buffer->memory_size = alloc_size;
    out_buffer->entrypoint = (ObeliskJitFunction)mem;
    out_buffer->target = target;
    out_buffer->code_bytes_emitted = code_bytes;
    return OBELISK_OK;
}

void obelisk_jit_buffer_free(ObeliskJitBuffer *buffer) {
    if (buffer && buffer->executable_memory) {
        munmap(buffer->executable_memory, buffer->memory_size);
        buffer->executable_memory = NULL;
        buffer->memory_size = 0;
        buffer->entrypoint = NULL;
        buffer->code_bytes_emitted = 0;
    }
}

ObeliskStatus obelisk_jit_execute(const ObeliskJitBuffer *buffer, ObeliskVM *vm) {
    if (!buffer || !buffer->entrypoint || !vm) {
        return OBELISK_ERR_JIT_COMPILATION;
    }

    uint8_t *tape_ptr = vm->tape.cells + vm->tape.head;
    uint8_t *final_ptr = buffer->entrypoint(tape_ptr, vm);

    if (final_ptr < vm->tape.cells || final_ptr >= vm->tape.cells + vm->tape.size) {
        return OBELISK_ERR_OUT_OF_BOUNDS;
    }

    vm->tape.head = (size_t)(final_ptr - vm->tape.cells);
    if (vm->tape.head > vm->tape.max_head_reached) {
        vm->tape.max_head_reached = vm->tape.head;
    }

    return OBELISK_OK;
}
