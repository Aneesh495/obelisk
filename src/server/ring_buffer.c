#include "obelisk/ring_buffer.h"
#include <stdlib.h>
#include <string.h>

bool obelisk_ring_buffer_init(ObeliskRingBuffer *rb, size_t capacity) {
    if (!rb) return false;
    if (capacity == 0) capacity = 65536;

    rb->buffer = (uint8_t *)malloc(capacity);
    if (!rb->buffer) return false;

    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->closed = false;

    if (pthread_mutex_init(&rb->lock, NULL) != 0) {
        free(rb->buffer);
        return false;
    }
    if (pthread_cond_init(&rb->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&rb->lock);
        free(rb->buffer);
        return false;
    }
    if (pthread_cond_init(&rb->not_full, NULL) != 0) {
        pthread_cond_destroy(&rb->not_empty);
        pthread_mutex_destroy(&rb->lock);
        free(rb->buffer);
        return false;
    }

    return true;
}

void obelisk_ring_buffer_free(ObeliskRingBuffer *rb) {
    if (!rb) return;
    pthread_mutex_lock(&rb->lock);
    rb->closed = true;
    pthread_cond_broadcast(&rb->not_empty);
    pthread_cond_broadcast(&rb->not_full);
    pthread_mutex_unlock(&rb->lock);

    pthread_mutex_destroy(&rb->lock);
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);

    if (rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
    }
    rb->capacity = 0;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

void obelisk_ring_buffer_close(ObeliskRingBuffer *rb) {
    if (!rb) return;
    pthread_mutex_lock(&rb->lock);
    rb->closed = true;
    pthread_cond_broadcast(&rb->not_empty);
    pthread_cond_broadcast(&rb->not_full);
    pthread_mutex_unlock(&rb->lock);
}

void obelisk_ring_buffer_reset(ObeliskRingBuffer *rb) {
    if (!rb) return;
    pthread_mutex_lock(&rb->lock);
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->closed = false;
    pthread_cond_broadcast(&rb->not_full);
    pthread_mutex_unlock(&rb->lock);
}

size_t obelisk_ring_buffer_write(ObeliskRingBuffer *rb, const uint8_t *data, size_t len) {
    if (!rb || !data || len == 0) return 0;

    pthread_mutex_lock(&rb->lock);
    size_t written = 0;

    while (written < len && !rb->closed) {
        while (rb->count == rb->capacity && !rb->closed) {
            pthread_cond_wait(&rb->not_full, &rb->lock);
        }
        if (rb->closed) break;

        size_t available = rb->capacity - rb->count;
        size_t to_write = (len - written < available) ? (len - written) : available;

        size_t first_chunk = rb->capacity - rb->head;
        if (to_write <= first_chunk) {
            memcpy(rb->buffer + rb->head, data + written, to_write);
            rb->head = (rb->head + to_write) % rb->capacity;
        } else {
            memcpy(rb->buffer + rb->head, data + written, first_chunk);
            memcpy(rb->buffer, data + written + first_chunk, to_write - first_chunk);
            rb->head = to_write - first_chunk;
        }

        rb->count += to_write;
        written += to_write;
        pthread_cond_signal(&rb->not_empty);
    }

    pthread_mutex_unlock(&rb->lock);
    return written;
}

size_t obelisk_ring_buffer_read(ObeliskRingBuffer *rb, uint8_t *out_data, size_t max_len) {
    if (!rb || !out_data || max_len == 0) return 0;

    pthread_mutex_lock(&rb->lock);
    while (rb->count == 0 && !rb->closed) {
        pthread_cond_wait(&rb->not_empty, &rb->lock);
    }

    if (rb->count == 0 && rb->closed) {
        pthread_mutex_unlock(&rb->lock);
        return 0;
    }

    size_t to_read = (max_len < rb->count) ? max_len : rb->count;
    size_t first_chunk = rb->capacity - rb->tail;

    if (to_read <= first_chunk) {
        memcpy(out_data, rb->buffer + rb->tail, to_read);
        rb->tail = (rb->tail + to_read) % rb->capacity;
    } else {
        memcpy(out_data, rb->buffer + rb->tail, first_chunk);
        memcpy(out_data + first_chunk, rb->buffer, to_read - first_chunk);
        rb->tail = to_read - first_chunk;
    }

    rb->count -= to_read;
    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->lock);
    return to_read;
}

int obelisk_ring_buffer_read_byte(ObeliskRingBuffer *rb) {
    if (!rb) return -1;
    pthread_mutex_lock(&rb->lock);
    while (rb->count == 0 && !rb->closed) {
        pthread_cond_wait(&rb->not_empty, &rb->lock);
    }
    if (rb->count == 0 && rb->closed) {
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }

    int byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;
    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->lock);
    return byte;
}

bool obelisk_ring_buffer_write_byte(ObeliskRingBuffer *rb, uint8_t byte) {
    if (!rb) return false;
    pthread_mutex_lock(&rb->lock);
    while (rb->count == rb->capacity && !rb->closed) {
        pthread_cond_wait(&rb->not_full, &rb->lock);
    }
    if (rb->closed) {
        pthread_mutex_unlock(&rb->lock);
        return false;
    }

    rb->buffer[rb->head] = byte;
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;
    pthread_cond_signal(&rb->not_empty);
    pthread_mutex_unlock(&rb->lock);
    return true;
}

size_t obelisk_ring_buffer_available_read(const ObeliskRingBuffer *rb) {
    if (!rb) return 0;
    return rb->count;
}

size_t obelisk_ring_buffer_available_write(const ObeliskRingBuffer *rb) {
    if (!rb) return 0;
    return rb->capacity - rb->count;
}
