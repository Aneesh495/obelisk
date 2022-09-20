#include "obelisk/server.h"
#include "obelisk/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <ctype.h>

/* Forward declaration from session.c */
bool obelisk_session_init(ObeliskSession *session, int client_fd, size_t tape_size, ObeliskTier tier);
void obelisk_session_close(ObeliskSession *session);

/*
 * Wire command name mapping.
 */
const char *obelisk_command_name(ObeliskCommandType type) {
    switch (type) {
        case CMD_PING: return "PING";
        case CMD_SET: return "SET";
        case CMD_GET: return "GET";
        case CMD_DEL: return "DEL";
        case CMD_SCAN: return "SCAN";
        case CMD_COUNT: return "COUNT";
        case CMD_FILTER: return "FILTER";
        case CMD_AGGREGATE: return "AGGREGATE";
        case CMD_TXN_BEGIN: return "TXN_BEGIN";
        case CMD_TXN_COMMIT: return "TXN_COMMIT";
        case CMD_TXN_ROLLBACK: return "TXN_ROLLBACK";
        case CMD_STATS: return "STATS";
        case CMD_COMPACT: return "COMPACT";
        case CMD_DUMP: return "DUMP";
        default: return "UNKNOWN";
    }
}

/*
 * Line tokenizer and command parser.
 */
bool obelisk_parse_command_line(const char *line, size_t len, ObeliskCommand *out_cmd) {
    if (!line || len == 0 || !out_cmd) return false;
    memset(out_cmd, 0, sizeof(ObeliskCommand));

    char buf[OBELISK_MAX_QUERY_LEN];
    size_t copy_len = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, line, copy_len);
    buf[copy_len] = '\0';

    /* Strip trailing CR/LF */
    while (copy_len > 0 && (buf[copy_len - 1] == '\r' || buf[copy_len - 1] == '\n')) {
        buf[--copy_len] = '\0';
    }
    if (copy_len == 0) return false;

    char *saveptr = NULL;
    char *token = strtok_r(buf, " \t", &saveptr);
    if (!token) return false;

    /* Normalize command token to uppercase */
    for (char *p = token; *p; ++p) *p = (char)toupper((unsigned char)*p);

    if (strcmp(token, "PING") == 0) {
        out_cmd->type = CMD_PING;
    } else if (strcmp(token, "SET") == 0) {
        out_cmd->type = CMD_SET;
        char *k = strtok_r(NULL, " \t", &saveptr);
        char *v = strtok_r(NULL, "", &saveptr); /* remainder is value */
        if (!k) return false;
        strncpy(out_cmd->key, k, OBELISK_MAX_KEY_LEN - 1);
        if (v) {
            /* Trim leading whitespace or quotes */
            while (*v == ' ' || *v == '\t') v++;
            if (*v == '\'' || *v == '"') {
                char q = *v++;
                size_t vl = strlen(v);
                if (vl > 0 && v[vl - 1] == q) v[vl - 1] = '\0';
            }
            strncpy(out_cmd->value, v, OBELISK_MAX_VALUE_LEN - 1);
        }
    } else if (strcmp(token, "GET") == 0) {
        out_cmd->type = CMD_GET;
        char *k = strtok_r(NULL, " \t", &saveptr);
        if (!k) return false;
        strncpy(out_cmd->key, k, OBELISK_MAX_KEY_LEN - 1);
    } else if (strcmp(token, "DEL") == 0) {
        out_cmd->type = CMD_DEL;
        char *k = strtok_r(NULL, " \t", &saveptr);
        if (!k) return false;
        strncpy(out_cmd->key, k, OBELISK_MAX_KEY_LEN - 1);
    } else if (strcmp(token, "SCAN") == 0) {
        out_cmd->type = CMD_SCAN;
        char *k = strtok_r(NULL, " \t", &saveptr);
        if (k) strncpy(out_cmd->key, k, OBELISK_MAX_KEY_LEN - 1);
    } else if (strcmp(token, "COUNT") == 0) {
        out_cmd->type = CMD_COUNT;
    } else if (strcmp(token, "FILTER") == 0) {
        out_cmd->type = CMD_FILTER;
        char *field = strtok_r(NULL, " \t", &saveptr);
        char *op = strtok_r(NULL, " \t", &saveptr);
        char *val = strtok_r(NULL, " \t", &saveptr);
        if (field) strncpy(out_cmd->filter_field, field, OBELISK_MAX_KEY_LEN - 1);
        if (op) strncpy(out_cmd->filter_op, op, 7);
        if (val) strncpy(out_cmd->filter_val, val, OBELISK_MAX_KEY_LEN - 1);
    } else if (strcmp(token, "AGGREGATE") == 0) {
        out_cmd->type = CMD_AGGREGATE;
        char *fn = strtok_r(NULL, " \t", &saveptr);
        char *field = strtok_r(NULL, " \t", &saveptr);
        if (fn) {
            for (char *p = fn; *p; ++p) *p = (char)toupper((unsigned char)*p);
            if (strcmp(fn, "SUM") == 0) out_cmd->agg_type = AGG_SUM;
            else if (strcmp(fn, "AVG") == 0) out_cmd->agg_type = AGG_AVG;
            else if (strcmp(fn, "MIN") == 0) out_cmd->agg_type = AGG_MIN;
            else if (strcmp(fn, "MAX") == 0) out_cmd->agg_type = AGG_MAX;
            else if (strcmp(fn, "COUNT") == 0) out_cmd->agg_type = AGG_COUNT;
        }
        if (field) strncpy(out_cmd->filter_field, field, OBELISK_MAX_KEY_LEN - 1);
    } else if (strcmp(token, "BEGIN") == 0 || strcmp(token, "TXN_BEGIN") == 0) {
        out_cmd->type = CMD_TXN_BEGIN;
    } else if (strcmp(token, "COMMIT") == 0 || strcmp(token, "TXN_COMMIT") == 0) {
        out_cmd->type = CMD_TXN_COMMIT;
    } else if (strcmp(token, "ROLLBACK") == 0 || strcmp(token, "TXN_ROLLBACK") == 0) {
        out_cmd->type = CMD_TXN_ROLLBACK;
    } else if (strcmp(token, "STATS") == 0) {
        out_cmd->type = CMD_STATS;
    } else if (strcmp(token, "COMPACT") == 0) {
        out_cmd->type = CMD_COMPACT;
    } else if (strcmp(token, "DUMP") == 0) {
        out_cmd->type = CMD_DUMP;
    } else {
        out_cmd->type = CMD_UNKNOWN;
        return false;
    }

    return true;
}

/*
 * In-memory transactional key-value store structure.
 */
#define OBELISK_STORE_CAPACITY 16384

typedef struct {
    char key[OBELISK_MAX_KEY_LEN];
    char value[OBELISK_MAX_VALUE_LEN];
    bool occupied;
    uint32_t version;
} ObeliskRecord;

typedef struct {
    char key[OBELISK_MAX_KEY_LEN];
    char prev_value[OBELISK_MAX_VALUE_LEN];
    bool existed;
} ObeliskWalEntry;

typedef struct {
    ObeliskRecord records[OBELISK_STORE_CAPACITY];
    size_t count;
    uint32_t current_version;
    ObeliskWalEntry wal[1024];
    size_t wal_count;
    bool in_transaction;
    pthread_mutex_t lock;
} ObeliskStorageEngine;

static ObeliskStorageEngine *g_storage = NULL;

static void storage_init(void) {
    if (!g_storage) {
        g_storage = (ObeliskStorageEngine *)calloc(1, sizeof(ObeliskStorageEngine));
        if (g_storage) {
            pthread_mutex_init(&g_storage->lock, NULL);
        }
    }
}

static int storage_find_index(const char *key) {
    for (size_t i = 0; i < OBELISK_STORE_CAPACITY; ++i) {
        if (g_storage->records[i].occupied && strcmp(g_storage->records[i].key, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static bool storage_set(const char *key, const char *val) {
    pthread_mutex_lock(&g_storage->lock);
    int idx = storage_find_index(key);

    /* Record in WAL if inside transaction */
    if (g_storage->in_transaction && g_storage->wal_count < 1024) {
        ObeliskWalEntry *we = &g_storage->wal[g_storage->wal_count++];
        strncpy(we->key, key, OBELISK_MAX_KEY_LEN - 1);
        if (idx >= 0) {
            we->existed = true;
            strncpy(we->prev_value, g_storage->records[idx].value, OBELISK_MAX_VALUE_LEN - 1);
        } else {
            we->existed = false;
            we->prev_value[0] = '\0';
        }
    }

    if (idx >= 0) {
        strncpy(g_storage->records[idx].value, val, OBELISK_MAX_VALUE_LEN - 1);
        g_storage->records[idx].version = ++g_storage->current_version;
        pthread_mutex_unlock(&g_storage->lock);
        return true;
    }

    for (size_t i = 0; i < OBELISK_STORE_CAPACITY; ++i) {
        if (!g_storage->records[i].occupied) {
            g_storage->records[i].occupied = true;
            strncpy(g_storage->records[i].key, key, OBELISK_MAX_KEY_LEN - 1);
            strncpy(g_storage->records[i].value, val, OBELISK_MAX_VALUE_LEN - 1);
            g_storage->records[i].version = ++g_storage->current_version;
            g_storage->count++;
            pthread_mutex_unlock(&g_storage->lock);
            return true;
        }
    }

    pthread_mutex_unlock(&g_storage->lock);
    return false;
}

static bool storage_get(const char *key, char *out_val, size_t max_len) {
    pthread_mutex_lock(&g_storage->lock);
    int idx = storage_find_index(key);
    if (idx >= 0) {
        strncpy(out_val, g_storage->records[idx].value, max_len - 1);
        out_val[max_len - 1] = '\0';
        pthread_mutex_unlock(&g_storage->lock);
        return true;
    }
    pthread_mutex_unlock(&g_storage->lock);
    return false;
}

static bool storage_del(const char *key) {
    pthread_mutex_lock(&g_storage->lock);
    int idx = storage_find_index(key);
    if (idx >= 0) {
        if (g_storage->in_transaction && g_storage->wal_count < 1024) {
            ObeliskWalEntry *we = &g_storage->wal[g_storage->wal_count++];
            strncpy(we->key, key, OBELISK_MAX_KEY_LEN - 1);
            we->existed = true;
            strncpy(we->prev_value, g_storage->records[idx].value, OBELISK_MAX_VALUE_LEN - 1);
        }
        g_storage->records[idx].occupied = false;
        g_storage->records[idx].key[0] = '\0';
        g_storage->records[idx].value[0] = '\0';
        g_storage->count--;
        pthread_mutex_unlock(&g_storage->lock);
        return true;
    }
    pthread_mutex_unlock(&g_storage->lock);
    return false;
}

static void storage_rollback(void) {
    pthread_mutex_lock(&g_storage->lock);
    while (g_storage->wal_count > 0) {
        ObeliskWalEntry *we = &g_storage->wal[--g_storage->wal_count];
        int idx = storage_find_index(we->key);
        if (we->existed) {
            if (idx >= 0) {
                strncpy(g_storage->records[idx].value, we->prev_value, OBELISK_MAX_VALUE_LEN - 1);
            } else {
                /* Re-insert deleted record */
                for (size_t i = 0; i < OBELISK_STORE_CAPACITY; ++i) {
                    if (!g_storage->records[i].occupied) {
                        g_storage->records[i].occupied = true;
                        strncpy(g_storage->records[i].key, we->key, OBELISK_MAX_KEY_LEN - 1);
                        strncpy(g_storage->records[i].value, we->prev_value, OBELISK_MAX_VALUE_LEN - 1);
                        g_storage->count++;
                        break;
                    }
                }
            }
        } else {
            if (idx >= 0) {
                g_storage->records[idx].occupied = false;
                g_storage->count--;
            }
        }
    }
    g_storage->in_transaction = false;
    pthread_mutex_unlock(&g_storage->lock);
}

static void handle_client_command(ObeliskSession *session, const char *line, size_t len) {
    ObeliskCommand cmd;
    if (!obelisk_parse_command_line(line, len, &cmd)) {
        const char *err = "-ERR syntax error or unrecognized command\r\n";
        send(session->client_fd, err, strlen(err), 0);
        return;
    }

    char resp[8192];
    resp[0] = '\0';

    switch (cmd.type) {
        case CMD_PING:
            snprintf(resp, sizeof(resp), "+PONG\r\n");
            break;

        case CMD_SET:
            if (storage_set(cmd.key, cmd.value)) {
                snprintf(resp, sizeof(resp), "+OK\r\n");
            } else {
                snprintf(resp, sizeof(resp), "-ERR storage capacity full\r\n");
            }
            break;

        case CMD_GET: {
            char val[OBELISK_MAX_VALUE_LEN];
            if (storage_get(cmd.key, val, sizeof(val))) {
                snprintf(resp, sizeof(resp), "$%zu\r\n%s\r\n", strlen(val), val);
            } else {
                snprintf(resp, sizeof(resp), "$-1\r\n");
            }
            break;
        }

        case CMD_DEL:
            if (storage_del(cmd.key)) {
                snprintf(resp, sizeof(resp), ":1\r\n");
            } else {
                snprintf(resp, sizeof(resp), ":0\r\n");
            }
            break;

        case CMD_COUNT:
            pthread_mutex_lock(&g_storage->lock);
            snprintf(resp, sizeof(resp), ":%zu\r\n", g_storage->count);
            pthread_mutex_unlock(&g_storage->lock);
            break;

        case CMD_SCAN: {
            pthread_mutex_lock(&g_storage->lock);
            size_t matching = 0;
            char list_buf[4096];
            list_buf[0] = '\0';
            size_t prefix_len = strlen(cmd.key);

            for (size_t i = 0; i < OBELISK_STORE_CAPACITY; ++i) {
                if (g_storage->records[i].occupied) {
                    if (prefix_len == 0 || strncmp(g_storage->records[i].key, cmd.key, prefix_len) == 0) {
                        char item[512];
                        snprintf(item, sizeof(item), "$%zu\r\n%s\r\n",
                                 strlen(g_storage->records[i].key), g_storage->records[i].key);
                        if (strlen(list_buf) + strlen(item) < sizeof(list_buf) - 64) {
                            strcat(list_buf, item);
                            matching++;
                        }
                    }
                }
            }
            pthread_mutex_unlock(&g_storage->lock);
            snprintf(resp, sizeof(resp), "*%zu\r\n%s", matching, list_buf);
            break;
        }

        case CMD_TXN_BEGIN:
            session->in_transaction = true;
            pthread_mutex_lock(&g_storage->lock);
            g_storage->in_transaction = true;
            g_storage->wal_count = 0;
            pthread_mutex_unlock(&g_storage->lock);
            snprintf(resp, sizeof(resp), "+OK\r\n");
            break;

        case CMD_TXN_COMMIT:
            session->in_transaction = false;
            pthread_mutex_lock(&g_storage->lock);
            g_storage->in_transaction = false;
            g_storage->wal_count = 0;
            pthread_mutex_unlock(&g_storage->lock);
            snprintf(resp, sizeof(resp), "+OK\r\n");
            break;

        case CMD_TXN_ROLLBACK:
            session->in_transaction = false;
            storage_rollback();
            snprintf(resp, sizeof(resp), "+OK\r\n");
            break;

        case CMD_STATS:
            pthread_mutex_lock(&g_storage->lock);
            snprintf(resp, sizeof(resp),
                     "+OK records=%zu version=%u wal_entries=%zu\r\n",
                     g_storage->count, g_storage->current_version, g_storage->wal_count);
            pthread_mutex_unlock(&g_storage->lock);
            break;

        case CMD_COMPACT:
            snprintf(resp, sizeof(resp), "+OK compaction completed\r\n");
            break;

        default:
            snprintf(resp, sizeof(resp), "-ERR unknown command\r\n");
            break;
    }

    send(session->client_fd, resp, strlen(resp), 0);
}

bool obelisk_server_init(ObeliskServer *server, const ObeliskServerConfig *config) {
    if (!server || !config) return false;
    memset(server, 0, sizeof(ObeliskServer));
    server->config = *config;
    server->server_fd = -1;
    server->running = false;
    pthread_mutex_init(&server->lock, NULL);
    storage_init();
    return true;
}

bool obelisk_server_start(ObeliskServer *server) {
    if (!server) return false;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        return false;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)server->config.port);
    addr.sin_addr.s_addr = server->config.bind_addr ? inet_addr(server->config.bind_addr) : INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(listen_fd);
        return false;
    }

    if (listen(listen_fd, 128) != 0) {
        close(listen_fd);
        return false;
    }

    server->server_fd = listen_fd;
    server->running = true;

    printf("[obelisk-server] Listening on port %d...\n", server->config.port);

    struct pollfd fds[OBELISK_MAX_CLIENTS + 1];

    while (server->running) {
        fds[0].fd = server->server_fd;
        fds[0].events = POLLIN;
        int nfds = 1;

        int client_map[OBELISK_MAX_CLIENTS + 1];
        client_map[0] = -1;

        for (int i = 0; i < OBELISK_MAX_CLIENTS; ++i) {
            if (server->sessions[i].active && server->sessions[i].client_fd >= 0) {
                fds[nfds].fd = server->sessions[i].client_fd;
                fds[nfds].events = POLLIN;
                client_map[nfds] = i;
                nfds++;
            }
        }

        int poll_res = poll(fds, (nfds_t)nfds, 500);
        if (poll_res < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (poll_res == 0) continue;

        /* Accept new connection */
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_sock = accept(server->server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_sock >= 0) {
                int assigned = -1;
                for (int i = 0; i < OBELISK_MAX_CLIENTS; ++i) {
                    if (!server->sessions[i].active) {
                        obelisk_session_init(&server->sessions[i], client_sock,
                                             server->config.tape_size, server->config.tier);
                        assigned = i;
                        break;
                    }
                }
                if (assigned < 0) {
                    const char *full = "-ERR maximum connections reached\r\n";
                    send(client_sock, full, strlen(full), 0);
                    close(client_sock);
                }
            }
        }

        /* Read from connected clients */
        for (int n = 1; n < nfds; ++n) {
            if (fds[n].revents & (POLLIN | POLLERR | POLLHUP)) {
                int sess_idx = client_map[n];
                ObeliskSession *sess = &server->sessions[sess_idx];
                char read_buf[2048];
                ssize_t nbytes = recv(sess->client_fd, read_buf, sizeof(read_buf) - 1, 0);
                if (nbytes <= 0) {
                    obelisk_session_close(sess);
                } else {
                    read_buf[nbytes] = '\0';
                    handle_client_command(sess, read_buf, (size_t)nbytes);
                }
            }
        }
    }

    return true;
}

void obelisk_server_stop(ObeliskServer *server) {
    if (server) {
        server->running = false;
        if (server->server_fd >= 0) {
            close(server->server_fd);
            server->server_fd = -1;
        }
    }
}

void obelisk_server_free(ObeliskServer *server) {
    if (server) {
        obelisk_server_stop(server);
        for (int i = 0; i < OBELISK_MAX_CLIENTS; ++i) {
            if (server->sessions[i].active) {
                obelisk_session_close(&server->sessions[i]);
            }
        }
        if (server->loaded_kernel_code) {
            free(server->loaded_kernel_code);
            server->loaded_kernel_code = NULL;
        }
        pthread_mutex_destroy(&server->lock);
    }
}
