#include "obelisk/server.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int session_vm_read_callback(void *user_data) {
    ObeliskSession *session = (ObeliskSession *)user_data;
    if (!session) return -1;
    return obelisk_ring_buffer_read_byte(&session->in_ring);
}

static int session_vm_write_callback(uint8_t byte, void *user_data) {
    ObeliskSession *session = (ObeliskSession *)user_data;
    if (!session) return -1;
    return obelisk_ring_buffer_write_byte(&session->out_ring, byte) ? 0 : -1;
}

bool obelisk_session_init(ObeliskSession *session, int client_fd, size_t tape_size, ObeliskTier tier) {
    if (!session) return false;
    memset(session, 0, sizeof(ObeliskSession));

    session->client_fd = client_fd;
    session->active = true;
    session->in_transaction = false;
    session->transaction_id = 0;

    if (!obelisk_ring_buffer_init(&session->in_ring, 65536)) {
        return false;
    }
    if (!obelisk_ring_buffer_init(&session->out_ring, 65536)) {
        obelisk_ring_buffer_free(&session->in_ring);
        return false;
    }

    if (obelisk_vm_init(&session->vm, tape_size, tier) != OBELISK_OK) {
        obelisk_ring_buffer_free(&session->in_ring);
        obelisk_ring_buffer_free(&session->out_ring);
        return false;
    }

    obelisk_vm_set_io(&session->vm, session_vm_read_callback, session_vm_write_callback, session);
    return true;
}

void obelisk_session_close(ObeliskSession *session) {
    if (!session || !session->active) return;
    session->active = false;
    if (session->client_fd >= 0) {
        close(session->client_fd);
        session->client_fd = -1;
    }
    obelisk_ring_buffer_close(&session->in_ring);
    obelisk_ring_buffer_close(&session->out_ring);
    obelisk_vm_free(&session->vm);
    obelisk_ring_buffer_free(&session->in_ring);
    obelisk_ring_buffer_free(&session->out_ring);
}
