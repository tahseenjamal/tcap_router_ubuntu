#include "transaction_table.h"

#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../config.h"

typedef struct tx_entry {

    uint32_t id;
    int backend;
    time_t ts;

    uint8_t gt[32];
    int gt_len;

    struct tx_entry* next;

} tx_entry_t;

static tx_entry_t* table[HASH_SIZE];

static pthread_mutex_t locks[HASH_SIZE];

static inline int hash(uint32_t id)
{
    return id % HASH_SIZE;
}

/* ------------------------------------------------ */

void tx_table_init()
{
    for (int i = 0; i < HASH_SIZE; i++) {

        table[i] = NULL;

        pthread_mutex_init(&locks[i], NULL);
    }
}

/* ------------------------------------------------ */

void tx_store(uint32_t otid, int backend)
{
    int h = hash(otid);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t* e = malloc(sizeof(tx_entry_t));

    if (!e) {
        pthread_mutex_unlock(&locks[h]);
        return;
    }

    e->id = otid;
    e->backend = backend;
    e->ts = time(NULL);

    e->next = table[h];

    table[h] = e;

    pthread_mutex_unlock(&locks[h]);
}

/* ------------------------------------------------ */

int tx_lookup(uint32_t dtid)
{
    int h = hash(dtid);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t* e = table[h];

    while (e) {

        if (e->id == dtid) {

            int backend = e->backend;

            pthread_mutex_unlock(&locks[h]);

            return backend;
        }

        e = e->next;
    }

    pthread_mutex_unlock(&locks[h]);

    return -1;
}

/* ------------------------------------------------ */

void tx_delete(uint32_t id)
{
    int h = hash(id);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t** cur = &table[h];

    while (*cur) {

        if ((*cur)->id == id) {

            tx_entry_t* tmp = *cur;

            *cur = tmp->next;

            free(tmp);

            break;
        }

        cur = &(*cur)->next;
    }

    pthread_mutex_unlock(&locks[h]);
}

/* ------------------------------------------------ */

static void* gc_thread(void* arg)
{
    (void)arg;

    while (1) {

        sleep(5);

        time_t now = time(NULL);

        for (int i = 0; i < HASH_SIZE; i++) {

            pthread_mutex_lock(&locks[i]);

            tx_entry_t** cur = &table[i];

            while (*cur) {

                if (now - (*cur)->ts > TX_TIMEOUT) {

                    tx_entry_t* tmp = *cur;

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

/* ------------------------------------------------ */

void tx_gc_start()
{
    pthread_t t;

    pthread_create(&t, NULL, gc_thread, NULL);

    pthread_detach(t);
}

void tx_store_full(uint32_t otid, int backend, uint8_t* gt, int gt_len)
{
    int h = hash(otid);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t* e = malloc(sizeof(tx_entry_t));

    if (!e) {
        pthread_mutex_unlock(&locks[h]);
        return;
    }

    e->id = otid;
    e->backend = backend;
    e->ts = time(NULL);

    e->gt_len = gt_len;
    if (gt && gt_len > 0)
        memcpy(e->gt, gt, gt_len);

    e->next = table[h];
    table[h] = e;

    pthread_mutex_unlock(&locks[h]);
}

int tx_lookup_full(uint32_t dtid, tx_info_t* out)
{
    int h = hash(dtid);

    pthread_mutex_lock(&locks[h]);

    tx_entry_t* e = table[h];

    while (e) {
        if (e->id == dtid) {

            out->backend = e->backend;
            out->gt_len = e->gt_len;

            memcpy(out->gt, e->gt, e->gt_len);

            pthread_mutex_unlock(&locks[h]);
            return 0;
        }
        e = e->next;
    }

    pthread_mutex_unlock(&locks[h]);
    return -1;
}



