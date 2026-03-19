#ifndef MSG_POOL_H
#define MSG_POOL_H

#include <osmocom/core/msgb.h>

void msg_pool_init();

struct msgb* msg_pool_get();
void msg_pool_put(struct msgb* msg);

#endif
