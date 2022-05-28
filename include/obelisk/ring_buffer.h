#ifndef OBELISK_RING_BUFFER_H
#define OBELISK_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Circular ring buffer designed for zero-copy streaming between
 * POSIX socket network streams and the Brainfuck virtual machine tape.
 */
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    bool closed;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ObeliskRingBuffer;

/*
 * Ring buffer lifecycle and operations.
 */
bool obelisk_ring_buffer_init(ObeliskRingBuffer *rb, size_t capacity);
void obelisk_ring_buffer_free(ObeliskRingBuffer *rb);
void obelisk_ring_buffer_close(ObeliskRingBuffer *rb);
void obelisk_ring_buffer_reset(ObeliskRingBuffer *rb);

/*
 * Data transmission operations (blocking and non-blocking).
 */
size_t obelisk_ring_buffer_write(ObeliskRingBuffer *rb, const uint8_t *data, size_t len);
size_t obelisk_ring_buffer_read(ObeliskRingBuffer *rb, uint8_t *out_data, size_t max_len);
int obelisk_ring_buffer_read_byte(ObeliskRingBuffer *rb);
bool obelisk_ring_buffer_write_byte(ObeliskRingBuffer *rb, uint8_t byte);

size_t obelisk_ring_buffer_available_read(const ObeliskRingBuffer *rb);
size_t obelisk_ring_buffer_available_write(const ObeliskRingBuffer *rb);

#ifdef __cplusplus
}
#endif

#endif /* OBELISK_RING_BUFFER_H */
