#ifndef ROUTER_H
#define ROUTER_H

#include <stdint.h>
#include "../core/msg.h"

void route_tcap(msg_t* msg, uint32_t otid, uint32_t dtid, int type);

#endif
