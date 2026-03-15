#ifndef ROUTER_H
#define ROUTER_H

#include <osmocom/core/msgb.h>
#include <stdint.h>

void route_tcap(struct msgb* msg, uint32_t otid, uint32_t dtid, int type);

#endif
