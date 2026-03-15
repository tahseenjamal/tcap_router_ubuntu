#include "worker_pool.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <string.h>

#include "../config.h"
#include "../router/router.h"

/* job structure */

typedef struct {
  struct msgb *msg;
  uint32_t otid;
  uint32_t dtid;
  int type;
} job_t;

/* one queue per worker */

static job_t queue[MAX_WORKERS][QUEUE_SIZE];

static atomic_ulong queue_drops = 0;
static atomic_uint head[MAX_WORKERS];
static atomic_uint tail[MAX_WORKERS];

/* pop job */

static job_t *queue_pop(int w) {
  unsigned int h = atomic_load_explicit(&head[w], memory_order_acquire);
  unsigned int t = atomic_load_explicit(&tail[w], memory_order_acquire);

  if (h == t)
    return NULL;

  job_t *job = &queue[w][h];

  atomic_store_explicit(&head[w], (h + 1) % QUEUE_SIZE, memory_order_release);

  return job;
}

/* enqueue specific worker */

void worker_enqueue_worker(int w, struct msgb *msg, uint32_t otid,
                           uint32_t dtid, int type) {
  unsigned int t = atomic_load_explicit(&tail[w], memory_order_acquire);
  unsigned int h = atomic_load_explicit(&head[w], memory_order_acquire);

  unsigned int next = (t + 1) % QUEUE_SIZE;

  if (next == h) {

    atomic_fetch_add(&queue_drops, 1);

    msgb_free(msg);

    return;
  }

  queue[w][t].msg = msg;
  queue[w][t].otid = otid;
  queue[w][t].dtid = dtid;
  queue[w][t].type = type;

  atomic_store_explicit(&tail[w], next, memory_order_release);
}

/* auto worker selection */

void worker_enqueue(struct msgb *msg, uint32_t otid, uint32_t dtid, int type) {
  int w = (otid ^ dtid) % MAX_WORKERS;

  worker_enqueue_worker(w, msg, otid, dtid, type);
}

/* worker thread */

static void *worker(void *arg) {
  int w = (intptr_t)arg;

  while (1) {
    job_t *job = queue_pop(w);

    if (!job) {
      sched_yield();
      continue;
    }

    if (!job->msg)
      continue;

    route_tcap(job->msg, job->otid, job->dtid, job->type);
  }

  return NULL;
}

/* initialize */

void worker_pool_init() {
  memset(queue, 0, sizeof(queue));

  for (int i = 0; i < MAX_WORKERS; i++) {
    atomic_store(&head[i], 0);
    atomic_store(&tail[i], 0);
  }

  pthread_t t;

  for (int i = 0; i < MAX_WORKERS; i++) {
    pthread_create(&t, NULL, worker, (void *)(intptr_t)i);
    pthread_detach(t);
  }
}
