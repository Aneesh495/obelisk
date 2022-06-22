#include "obelisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_jit_target_detection(void) {
    printf("[test_jit] Detecting host JIT architecture target...\n");
    ObeliskJitTarget target = obelisk_jit_detect_host_target();
    assert(target == JIT_TARGET_ARM64 || target == JIT_TARGET_X86_64 || target == JIT_TARGET_NONE);
    printf("  Host Target: %s\n",
           target == JIT_TARGET_ARM64 ? "ARM64" :
           target == JIT_TARGET_X86_64 ? "x86-64" : "NONE");
    printf("  PASSED\n");
}

static void test_jit_arithmetic_execution(void) {
    printf("[test_jit] Testing JIT machine code compilation and execution...\n");
    ObeliskJitTarget target = obelisk_jit_detect_host_target();
    if (target == JIT_TARGET_NONE) {
        printf("  SKIPPED: Architecture does not have JIT backend enabled.\n");
        return;
    }

    const char *src = "+++++>+++<++";
    ObeliskProgram *prog = NULL;
    assert(obelisk_parse_source(src, strlen(src), &prog));

    ObeliskJitBuffer jit_buf;
    memset(&jit_buf, 0, sizeof(jit_buf));
    assert(obelisk_jit_compile(prog, &jit_buf) == OBELISK_OK);
    assert(jit_buf.entrypoint != NULL);
    assert(jit_buf.code_bytes_emitted > 0);

    ObeliskVM vm;
    obelisk_vm_init(&vm, 1024, OBELISK_TIER_JIT);
    assert(obelisk_jit_execute(&jit_buf, &vm) == OBELISK_OK);

    assert(vm.tape.cells[0] == 7);
    assert(vm.tape.cells[1] == 3);
    assert(vm.tape.head == 0);

    obelisk_jit_buffer_free(&jit_buf);
    obelisk_vm_free(&vm);
    obelisk_program_free(prog);
    printf("  PASSED\n");
}

static void test_jit_loop_execution(void) {
    printf("[test_jit] Testing JIT loop execution (5 * 4 = 20)...\n");
    ObeliskJitTarget target = obelisk_jit_detect_host_target();
    if (target == JIT_TARGET_NONE) {
        printf("  SKIPPED: JIT not enabled.\n");
        return;
    }

    const char *src = "+++++[>++++<-]>";
    ObeliskProgram *prog = NULL;
    assert(obelisk_parse_source(src, strlen(src), &prog));

    ObeliskJitBuffer jit_buf;
    memset(&jit_buf, 0, sizeof(jit_buf));
    assert(obelisk_jit_compile(prog, &jit_buf) == OBELISK_OK);

    ObeliskVM vm;
    obelisk_vm_init(&vm, 1024, OBELISK_TIER_JIT);
    assert(obelisk_jit_execute(&jit_buf, &vm) == OBELISK_OK);

    assert(vm.tape.cells[0] == 0);
    assert(vm.tape.cells[1] == 20);
    assert(vm.tape.head == 1);

    obelisk_jit_buffer_free(&jit_buf);
    obelisk_vm_free(&vm);
    obelisk_program_free(prog);
    printf("  PASSED\n");
}

int main(void) {
    printf("=== Obelisk JIT Unit Tests ===\n");
    test_jit_target_detection();
    test_jit_arithmetic_execution();
    test_jit_loop_execution();
    printf("All Obelisk JIT unit tests passed successfully!\n");
    return 0;
}
