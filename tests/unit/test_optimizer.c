#include "obelisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_arithmetic_coalescing(void) {
    printf("[test_optimizer] Testing arithmetic coalescing...\n");
    const char *src = "+++++";
    ObeliskProgram *prog = NULL;
    assert(obelisk_parse_and_optimize(src, strlen(src), OPT_PASS_COALESCE_ARITHMETIC, &prog));
    assert(prog->count == 1);
    assert(prog->instructions[0].opcode == IR_ADD);
    assert(prog->instructions[0].val == 5);
    obelisk_program_free(prog);
    printf("  PASSED\n");
}

static void test_pointer_shift_coalescing(void) {
    printf("[test_optimizer] Testing pointer shift coalescing...\n");
    const char *src = ">>>>";
    ObeliskProgram *prog = NULL;
    assert(obelisk_parse_and_optimize(src, strlen(src), OPT_PASS_COALESCE_SHIFTS, &prog));
    assert(prog->count == 1);
    assert(prog->instructions[0].opcode == IR_MOVE);
    assert(prog->instructions[0].val == 4);
    obelisk_program_free(prog);
    printf("  PASSED\n");
}

static void test_clear_loop_folding(void) {
    printf("[test_optimizer] Testing clear-loop folding ([-] -> SET 0)...\n");
    const char *src = "+++++[-]";
    ObeliskProgram *prog = NULL;
    assert(obelisk_parse_and_optimize(src, strlen(src), OPT_PASS_ALL, &prog));
    /* Expect: IR_ADD 5, IR_SET 0 */
    assert(prog->count == 2);
    assert(prog->instructions[0].opcode == IR_ADD);
    assert(prog->instructions[0].val == 5);
    assert(prog->instructions[1].opcode == IR_SET);
    assert(prog->instructions[1].val == 0);

    /* Verify execution */
    ObeliskVM vm;
    obelisk_vm_init(&vm, 1024, OBELISK_TIER_OPTIMIZED);
    assert(obelisk_vm_run_program(&vm, prog) == OBELISK_OK);
    assert(vm.tape.cells[0] == 0);
    obelisk_vm_free(&vm);

    obelisk_program_free(prog);
    printf("  PASSED\n");
}

static void test_multiply_add_folding(void) {
    printf("[test_optimizer] Testing multiply-add loop folding ([->+++<])...\n");
    const char *src = "++++[->+++<]>";
    ObeliskProgram *prog = NULL;
    assert(obelisk_parse_and_optimize(src, strlen(src), OPT_PASS_ALL, &prog));

    ObeliskVM vm;
    obelisk_vm_init(&vm, 1024, OBELISK_TIER_OPTIMIZED);
    assert(obelisk_vm_run_program(&vm, prog) == OBELISK_OK);
    /* 4 * 3 = 12 */
    assert(vm.tape.cells[0] == 0);
    assert(vm.tape.cells[1] == 12);
    assert(vm.tape.head == 1);
    obelisk_vm_free(&vm);

    obelisk_program_free(prog);
    printf("  PASSED\n");
}

static void test_zero_scan_folding(void) {
    printf("[test_optimizer] Testing zero-scan loop folding ([>])...\n");
    const char *src = "+++>++>+>>+[>]";
    ObeliskProgram *prog = NULL;
    assert(obelisk_parse_and_optimize(src, strlen(src), OPT_PASS_ALL, &prog));

    ObeliskVM vm;
    obelisk_vm_init(&vm, 1024, OBELISK_TIER_OPTIMIZED);
    assert(obelisk_vm_run_program(&vm, prog) == OBELISK_OK);
    /* Should scan forward to the first zero cell after head */
    assert(vm.tape.cells[vm.tape.head] == 0);
    obelisk_vm_free(&vm);

    obelisk_program_free(prog);
    printf("  PASSED\n");
}

int main(void) {
    printf("=== Obelisk Optimizer Unit Tests ===\n");
    test_arithmetic_coalescing();
    test_pointer_shift_coalescing();
    test_clear_loop_folding();
    test_multiply_add_folding();
    test_zero_scan_folding();
    printf("All Obelisk Optimizer unit tests passed successfully!\n");
    return 0;
}
