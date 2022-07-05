#include "obelisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_ring_buffer_basic(void) {
    printf("[test_ring_buffer] Testing basic write and read...\n");
    ObeliskRingBuffer rb;
    assert(obelisk_ring_buffer_init(&rb, 16));
    assert(obelisk_ring_buffer_available_read(&rb) == 0);
    assert(obelisk_ring_buffer_available_write(&rb) == 16);

    const char *msg = "hello";
    size_t w = obelisk_ring_buffer_write(&rb, (const uint8_t *)msg, 5);
    assert(w == 5);
    assert(obelisk_ring_buffer_available_read(&rb) == 5);

    uint8_t out[16];
    size_t r = obelisk_ring_buffer_read(&rb, out, 5);
    assert(r == 5);
    assert(memcmp(out, "hello", 5) == 0);
    assert(obelisk_ring_buffer_available_read(&rb) == 0);

    obelisk_ring_buffer_free(&rb);
    printf("  PASSED\n");
}

static void test_ring_buffer_wraparound(void) {
    printf("[test_ring_buffer] Testing buffer boundary wrap-around...\n");
    ObeliskRingBuffer rb;
    assert(obelisk_ring_buffer_init(&rb, 8));

    /* Write 6 bytes, read 4 bytes */
    obelisk_ring_buffer_write(&rb, (const uint8_t *)"123456", 6);
    uint8_t out[8];
    obelisk_ring_buffer_read(&rb, out, 4);
    assert(memcmp(out, "1234", 4) == 0);

    /* Write 5 more bytes (causes wrap-around across capacity 8) */
    obelisk_ring_buffer_write(&rb, (const uint8_t *)"ABCDE", 5);
    assert(obelisk_ring_buffer_available_read(&rb) == 7); /* "56ABCDE" */

    size_t r = obelisk_ring_buffer_read(&rb, out, 7);
    assert(r == 7);
    assert(memcmp(out, "56ABCDE", 7) == 0);

    obelisk_ring_buffer_free(&rb);
    printf("  PASSED\n");
}

static void test_ring_buffer_byte_ops(void) {
    printf("[test_ring_buffer] Testing byte-level operations...\n");
    ObeliskRingBuffer rb;
    assert(obelisk_ring_buffer_init(&rb, 8));

    assert(obelisk_ring_buffer_write_byte(&rb, 'X'));
    assert(obelisk_ring_buffer_write_byte(&rb, 'Y'));
    assert(obelisk_ring_buffer_read_byte(&rb) == 'X');
    assert(obelisk_ring_buffer_read_byte(&rb) == 'Y');

    obelisk_ring_buffer_close(&rb);
    assert(obelisk_ring_buffer_read_byte(&rb) == -1);

    obelisk_ring_buffer_free(&rb);
    printf("  PASSED\n");
}

int main(void) {
    printf("=== Obelisk Ring Buffer Unit Tests ===\n");
    test_ring_buffer_basic();
    test_ring_buffer_wraparound();
    test_ring_buffer_byte_ops();
    printf("All Obelisk Ring Buffer unit tests passed successfully!\n");
    return 0;
}
