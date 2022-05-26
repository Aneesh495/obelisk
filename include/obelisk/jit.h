#ifndef OBELISK_JIT_H
#define OBELISK_JIT_H

#include "obelisk/vm.h"
#include "obelisk/ir.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Native JIT function signature.
 * Executes natively generated machine code directly over the tape pointer,
 * invoking C callback bridges for I/O operations and step limit enforcement.
 */
typedef uint8_t* (*ObeliskJitFunction)(uint8_t *tape_ptr, void *io_ctx);

/*
 * Target machine architectures supported by the JIT backend.
 */
typedef enum {
    JIT_TARGET_NONE = 0,
    JIT_TARGET_ARM64 = 1,  /* Apple Silicon / ARM64 (AArch64) */
    JIT_TARGET_X86_64 = 2  /* x86-64 / AMD64 */
} ObeliskJitTarget;

/*
 * JIT compiled executable buffer container.
 */
typedef struct {
    void *executable_memory;
    size_t memory_size;
    ObeliskJitFunction entrypoint;
    ObeliskJitTarget target;
    size_t code_bytes_emitted;
} ObeliskJitBuffer;

/*
 * JIT compilation and execution interfaces.
 */
ObeliskJitTarget obelisk_jit_detect_host_target(void);
ObeliskStatus obelisk_jit_compile(const ObeliskProgram *program, ObeliskJitBuffer *out_buffer);
void obelisk_jit_buffer_free(ObeliskJitBuffer *buffer);
ObeliskStatus obelisk_jit_execute(const ObeliskJitBuffer *buffer, ObeliskVM *vm);

#ifdef __cplusplus
}
#endif

#endif /* OBELISK_JIT_H */
