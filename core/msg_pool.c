#include "msg_pool.h"

#include <osmocom/core/msgb.h>
#include <stdatomic.h>

#define POOL_SIZE 65536

static struct msgb* pool[POOL_SIZE];
static atomic_int top = 0;

void msg_pool_init() {
    for (int i = 0; i < POOL_SIZE; i++) {
        pool[i] = msgb_alloc(4096, "pool");
    }
    atomic_store(&top, POOL_SIZE);
}

struct msgb* msg_pool_get() {
    int t = atomic_fetch_sub(&top, 1);
    if (t <= 0) return msgb_alloc(4096, "fallback");
    return pool[t - 1];
}

void msg_pool_put(struct msgb* msg) {
    int t = atomic_fetch_add(&top, 1);
    if (t < POOL_SIZE)
        pool[t] = msg;
    else
        msgb_free(msg);
}
