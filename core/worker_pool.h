#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include <osmocom/core/msgb.h>
#include <stdint.h>

void worker_pool_init();

/* enqueue automatically chooses worker */
void worker_enqueue(struct msgb* msg, uint32_t otid, uint32_t dtid, int type);

/* enqueue to specific worker */
void worker_enqueue_worker(int worker, struct msgb* msg, uint32_t otid,
                           uint32_t dtid, int type);

#endif
