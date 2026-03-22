#ifndef MSG_H
#define MSG_H

#include <stdint.h>

typedef struct {
    uint8_t data[4096];
    int len;
    uint8_t cb[32];
} msg_t;

#endif
