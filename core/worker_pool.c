#include "worker_pool.h"

#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

#include "../config.h"
#include "../router/router.h"
#include "msg_pool.h"

typedef struct {
    struct msgb *msg;
    uint32_t otid;
    uint32_t dtid;
    int type;
} job_t;

static job_t queue[MAX_WORKERS][QUEUE_SIZE];

static atomic_uint head[MAX_WORKERS];
static atomic_uint tail[MAX_WORKERS];

static pthread_cond_t cond[MAX_WORKERS];
static pthread_mutex_t lock[MAX_WORKERS];

/* ========================================= */

static job_t *queue_pop(int w) {
    unsigned int h = atomic_load(&head[w]);
    unsigned int t = atomic_load(&tail[w]);

    if (h == t) return NULL;

    job_t *job = &queue[w][h];

    atomic_store(&head[w], (h + 1) % QUEUE_SIZE);

    return job;
}

/* ========================================= */

void worker_enqueue(struct msgb *msg, uint32_t otid, uint32_t dtid, int type) {
    int w = (otid ? otid : dtid) % MAX_WORKERS;

    pthread_mutex_lock(&lock[w]);

    unsigned int t = atomic_load(&tail[w]);
    unsigned int h = atomic_load(&head[w]);
    unsigned int next = (t + 1) % QUEUE_SIZE;

    /* ✅ OVERFLOW PROTECTION */
    if (next == h) {
        printf("Queue full → dropping message\n");
        msg_pool_put(msg);
        pthread_mutex_unlock(&lock[w]);
        return;
    }

    queue[w][t].msg = msg;
    queue[w][t].otid = otid;
    queue[w][t].dtid = dtid;
    queue[w][t].type = type;

    atomic_store(&tail[w], next);

    pthread_cond_signal(&cond[w]);

    pthread_mutex_unlock(&lock[w]);
}

/* ========================================= */

static void *worker(void *arg) {
    int w = (intptr_t)arg;

    while (1) {
        pthread_mutex_lock(&lock[w]);

        while (atomic_load(&head[w]) == atomic_load(&tail[w])) {
            pthread_cond_wait(&cond[w], &lock[w]);
        }

        pthread_mutex_unlock(&lock[w]);

        job_t *job = queue_pop(w);

        if (!job || !job->msg) continue;

        route_tcap(job->msg, job->otid, job->dtid, job->type);
    }

    return NULL;
}

/* ========================================= */

void worker_pool_init() {
    memset(queue, 0, sizeof(queue));

    for (int i = 0; i < MAX_WORKERS; i++) {
        atomic_store(&head[i], 0);
        atomic_store(&tail[i], 0);

        pthread_mutex_init(&lock[i], NULL);
        pthread_cond_init(&cond[i], NULL);

        pthread_t t;
        pthread_create(&t, NULL, worker, (void *)(intptr_t)i);
        pthread_detach(t);
    }
}
