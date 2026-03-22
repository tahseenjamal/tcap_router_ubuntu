#include "msg_pool.h"
#include "msg.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define POOL_SIZE 65536

static msg_t *pool[POOL_SIZE];
static atomic_int top = 0;

/* ========================================= */

void msg_pool_init() {
  for (int i = 0; i < POOL_SIZE; i++) {
    pool[i] = malloc(sizeof(msg_t));
  }
  atomic_store(&top, POOL_SIZE);
}

/* ========================================= */

msg_t *msg_pool_get() {
  int t = atomic_fetch_sub(&top, 1);

  if (t <= 0) {
    return malloc(sizeof(msg_t)); // fallback
  }

  return pool[t - 1];
}

/* ========================================= */

void msg_pool_put(msg_t *msg) {
  if (!msg)
    return;

  int t = atomic_fetch_add(&top, 1);

  if (t < POOL_SIZE) {
    pool[t] = msg;
  } else {
    free(msg);
  }
}
