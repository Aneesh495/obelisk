#include "obelisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int io_in_buf[256];
static size_t io_in_pos = 0;
static size_t io_in_len = 0;

static uint8_t io_out_buf[256];
static size_t io_out_len = 0;

static int mock_read(void *u) {
    (void)u;
    if (io_in_pos < io_in_len) {
        return io_in_buf[io_in_pos++];
    }
    return -1;
}

static int mock_write(uint8_t b, void *u) {
    (void)u;
    if (io_out_len < sizeof(io_out_buf)) {
        io_out_buf[io_out_len++] = b;
        return 0;
    }
    return -1;
}

static void test_tape_allocation(void) {
    printf("[test_vm] Testing tape allocation and bounds...\n");
    ObeliskTape tape;
    ObeliskStatus st = obelisk_tape_init(&tape, 1024, false);
    assert(st == OBELISK_OK);
    assert(tape.cells != NULL);
    assert(tape.size == 1024);
    assert(tape.head == 0);

    /* Write and verify */
    tape.cells[0] = 42;
    tape.cells[1023] = 99;
    assert(tape.cells[0] == 42);
    assert(tape.cells[1023] == 99);

    obelisk_tape_reset(&tape);
    assert(tape.cells[0] == 0);
    assert(tape.cells[1023] == 0);

    obelisk_tape_free(&tape);
    assert(tape.cells == NULL);
    printf("  PASSED\n");
}

static void test_vm_arithmetic(void) {
    printf("[test_vm] Testing basic arithmetic and pointer moves...\n");
    ObeliskVM vm;
    assert(obelisk_vm_init(&vm, 1024, OBELISK_TIER_INTERPRETER) == OBELISK_OK);

    /* Code: +++ > ++ < + */
    const char *src = "+++>++<+";
    assert(obelisk_vm_run_source(&vm, src, strlen(src)) == OBELISK_OK);
    assert(vm.tape.cells[0] == 4);
    assert(vm.tape.cells[1] == 2);
    assert(vm.tape.head == 0);

    obelisk_vm_free(&vm);
    printf("  PASSED\n");
}

static void test_vm_loop_multiplication(void) {
    printf("[test_vm] Testing multiplication loop: 6 * 7 = 42...\n");
    ObeliskVM vm;
    assert(obelisk_vm_init(&vm, 1024, OBELISK_TIER_INTERPRETER) == OBELISK_OK);

    /* Code: ++++++ [ > +++++++ < - ] > */
    const char *src = "++++++[>+++++++<-]>";
    assert(obelisk_vm_run_source(&vm, src, strlen(src)) == OBELISK_OK);
    assert(vm.tape.cells[0] == 0);
    assert(vm.tape.cells[1] == 42);
    assert(vm.tape.head == 1);

    obelisk_vm_free(&vm);
    printf("  PASSED\n");
}

static void test_vm_io_streaming(void) {
    printf("[test_vm] Testing I/O streaming (Hello World)...\n");
    ObeliskVM vm;
    assert(obelisk_vm_init(&vm, 1024, OBELISK_TIER_INTERPRETER) == OBELISK_OK);

    io_out_len = 0;
    obelisk_vm_set_io(&vm, mock_read, mock_write, NULL);

    /* Canonical non-negative tape Hello World */
    const char *src = ">++++++++[<+++++++++>-]<.>++++[<+++++++>-]<+.+++++++..+++.>>++++++[<+++++++>-]<++.------------.>++++++[<+++++++++>-]<+.<.+++.------.--------.>>>++++[<++++++++>-]<+.";
    ObeliskStatus st = obelisk_vm_run_source(&vm, src, strlen(src));
    assert(st == OBELISK_OK);

    assert(io_out_len == 13);
    assert(memcmp(io_out_buf, "Hello, World!", 13) == 0);

    obelisk_vm_free(&vm);
    printf("  PASSED\n");
}

int main(void) {
    printf("=== Obelisk VM Unit Tests ===\n");
    test_tape_allocation();
    test_vm_arithmetic();
    test_vm_loop_multiplication();
    test_vm_io_streaming();
    printf("All Obelisk VM unit tests passed successfully!\n");
    return 0;
}
