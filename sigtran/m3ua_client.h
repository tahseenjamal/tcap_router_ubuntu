#pragma once

#include <stdint.h>

extern int m3ua_sock;

int m3ua_send(uint8_t *buf, int len);
