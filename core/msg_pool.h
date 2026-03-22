#ifndef MSG_POOL_H
#define MSG_POOL_H

#include "msg.h"

void msg_pool_init();

msg_t* msg_pool_get();
void msg_pool_put(msg_t* msg);

#endif
