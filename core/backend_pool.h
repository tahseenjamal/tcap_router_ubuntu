#ifndef BACKEND_POOL_H
#define BACKEND_POOL_H

#include <stdatomic.h>

typedef struct {
    int id;
    atomic_int active;
    int sock;
    atomic_int load;
} backend_t;

void backend_pool_init();
backend_t* backend_choose();
backend_t* backend_get(int id);

#endif
