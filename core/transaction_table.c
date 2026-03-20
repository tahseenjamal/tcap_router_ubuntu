#include "transaction_table.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../config.h"

/* =========================================================
 * STRUCT
 * ========================================================= */

typedef struct tx_entry {
    uint32_t id;
    int backend;
    time_t ts;

    uint8_t gt[32];
    int gt_len;

    struct tx_entry *next;

} tx_entry_t;

/* =========================================================
 * GLOBALS
 * ========================================================= */

static tx_entry_t *table[HASH_SIZE];
static pthread_mutex_t locks[HASH_SIZE];

/* =========================================================
 * HASH
 * ========================================================= */

static inline int hash(uint32_t id) { return id % HASH_SIZE; }

/* =========================================================
 * INIT
 * ========================================================= */

void tx_table_init() {
    for (int i = 0; i < HASH_SIZE; i++) {
        table[i] = NULL;
        pthread_mutex_init(&locks[i], NULL);
    }
}

/* =========================================================
 * STORE (MINIMAL)
 * ========================================================= */

void tx_store(uint32_t otid, int backend) {
    int h = hash(otid);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t *e = table[h];

    /* ✅ FIX: check existing */
    while (e) {
        if (e->id == otid) {
            e->backend = backend;
            e->ts = time(NULL);
            pthread_mutex_unlock(&locks[h]);
            return;
        }
        e = e->next;
    }

    /* create new */
    e = malloc(sizeof(tx_entry_t));
    if (!e) {
        pthread_mutex_unlock(&locks[h]);
        return;
    }

    e->id = otid;
    e->backend = backend;
    e->ts = time(NULL);

    e->gt_len = 0;

    e->next = table[h];
    table[h] = e;

    pthread_mutex_unlock(&locks[h]);
}

/* =========================================================
 * STORE (FULL)
 * ========================================================= */

void tx_store_full(uint32_t otid, int backend, uint8_t *gt, int gt_len) {
    int h = hash(otid);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t *e = table[h];

    /* ✅ FIX: update existing */
    while (e) {
        if (e->id == otid) {
            e->backend = backend;
            e->ts = time(NULL);

            if (gt && gt_len > 0) {
                if (gt_len > 32) gt_len = 32;  // ✅ FIX
                e->gt_len = gt_len;
                memcpy(e->gt, gt, gt_len);
            }

            pthread_mutex_unlock(&locks[h]);
            return;
        }
        e = e->next;
    }

    /* create new */
    e = malloc(sizeof(tx_entry_t));
    if (!e) {
        pthread_mutex_unlock(&locks[h]);
        return;
    }

    e->id = otid;
    e->backend = backend;
    e->ts = time(NULL);

    if (gt && gt_len > 0) {
        if (gt_len > 32) gt_len = 32;  // ✅ FIX
        e->gt_len = gt_len;
        memcpy(e->gt, gt, gt_len);
    } else {
        e->gt_len = 0;
    }

    e->next = table[h];
    table[h] = e;

    pthread_mutex_unlock(&locks[h]);
}

/* =========================================================
 * LOOKUP (MINIMAL)
 * ========================================================= */

int tx_lookup(uint32_t dtid) {
    int h = hash(dtid);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t *e = table[h];

    while (e) {
        if (e->id == dtid) {
            e->ts = time(NULL);  // ✅ CRITICAL FIX

            int backend = e->backend;

            pthread_mutex_unlock(&locks[h]);
            return backend;
        }

        e = e->next;
    }

    pthread_mutex_unlock(&locks[h]);
    return -1;
}

/* =========================================================
 * LOOKUP (FULL)
 * ========================================================= */

int tx_lookup_full(uint32_t dtid, tx_info_t *out) {
    int h = hash(dtid);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t *e = table[h];

    while (e) {
        if (e->id == dtid) {
            e->ts = time(NULL);  // ✅ CRITICAL FIX

            out->backend = e->backend;
            out->gt_len = e->gt_len;

            if (e->gt_len > 0) memcpy(out->gt, e->gt, e->gt_len);

            pthread_mutex_unlock(&locks[h]);
            return 0;
        }

        e = e->next;
    }

    pthread_mutex_unlock(&locks[h]);
    return -1;
}

/* =========================================================
 * DELETE
 * ========================================================= */

void tx_delete(uint32_t id) {
    int h = hash(id);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t **cur = &table[h];

    while (*cur) {
        if ((*cur)->id == id) {
            tx_entry_t *tmp = *cur;
            *cur = tmp->next;

            free(tmp);
            break;
        }

        cur = &(*cur)->next;
    }

    pthread_mutex_unlock(&locks[h]);
}

/* =========================================================
 * GC THREAD
 * ========================================================= */

static void *gc_thread(void *arg) {
    (void)arg;

    while (1) {
        sleep(5);

        time_t now = time(NULL);

        for (int i = 0; i < HASH_SIZE; i++) {
            pthread_mutex_lock(&locks[i]);

            tx_entry_t **cur = &table[i];

            while (*cur) {
                if (now - (*cur)->ts > TX_TIMEOUT) {
                    tx_entry_t *tmp = *cur;

                    printf("TX TIMEOUT id=%u backend=%d age=%ld sec\n", tmp->id,
                           tmp->backend, now - tmp->ts);

                    *cur = tmp->next;
                    free(tmp);

                } else {
                    cur = &(*cur)->next;
                }
            }

            pthread_mutex_unlock(&locks[i]);
        }
    }

    return NULL;
}

/* =========================================================
 * START GC
 * ========================================================= */

void tx_gc_start() {
    pthread_t t;

    if (pthread_create(&t, NULL, gc_thread, NULL) != 0) {
        perror("gc thread create failed");
        return;
    }

    pthread_detach(t);
}
