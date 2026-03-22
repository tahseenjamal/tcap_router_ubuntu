#ifndef M3UA_H
#define M3UA_H

#include <stdint.h>

typedef struct {
    uint8_t version;
    uint8_t reserved;
    uint8_t msg_class;
    uint8_t msg_type;
    uint32_t length;
} __attribute__((packed)) m3ua_hdr_t;

#define M3UA_VERSION 1

/* Classes */
#define M3UA_CLASS_TRANSFER 1
#define M3UA_CLASS_ASPSM 3
#define M3UA_CLASS_ASPTM 4

/* Types */
#define M3UA_DATA 1
#define M3UA_ASPUP 1
#define M3UA_ASPUP_ACK 4
#define M3UA_ASPAC 1
#define M3UA_ASPAC_ACK 3

#endif
