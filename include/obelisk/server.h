#ifndef OBELISK_SERVER_H
#define OBELISK_SERVER_H

#include "obelisk/vm.h"
#include "obelisk/ring_buffer.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OBELISK_DEFAULT_PORT 7379
#define OBELISK_MAX_CLIENTS 64

/*
 * Client session state representing an active connection to the database.
 */
typedef struct {
    int client_fd;
    bool active;
    bool in_transaction;
    uint32_t transaction_id;
    ObeliskVM vm;
    ObeliskRingBuffer in_ring;
    ObeliskRingBuffer out_ring;
    pthread_t thread;
} ObeliskSession;

/*
 * Configuration options for the Obelisk server daemon.
 */
typedef struct {
    int port;
    const char *bind_addr;
    const char *bf_kernel_path;
    size_t tape_size;
    ObeliskTier tier;
    bool verbose;
} ObeliskServerConfig;

/*
 * Master server daemon state.
 */
typedef struct {
    int server_fd;
    bool running;
    ObeliskServerConfig config;
    ObeliskSession sessions[OBELISK_MAX_CLIENTS];
    pthread_mutex_t lock;
    char *loaded_kernel_code;
    size_t loaded_kernel_len;
} ObeliskServer;

/*
 * Server daemon lifecycle.
 */
bool obelisk_server_init(ObeliskServer *server, const ObeliskServerConfig *config);
bool obelisk_server_start(ObeliskServer *server);
void obelisk_server_stop(ObeliskServer *server);
void obelisk_server_free(ObeliskServer *server);

#ifdef __cplusplus
}
#endif

#endif /* OBELISK_SERVER_H */
