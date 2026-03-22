#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include <stdint.h>
#include "msg.h"

void worker_pool_init();

void worker_enqueue(msg_t* msg, uint32_t otid, uint32_t dtid, int type);

#endif
