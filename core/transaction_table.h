#ifndef TX_TABLE_H
#define TX_TABLE_H

#include <stdint.h>

typedef struct {
    int backend;

    uint8_t gt[32];
    int gt_len;

} tx_info_t;

void tx_store_full(uint32_t otid, int backend, uint8_t* gt, int gt_len);
int tx_lookup_full(uint32_t dtid, tx_info_t* out);

void tx_table_init();

void tx_store(uint32_t otid, int backend);

int tx_lookup(uint32_t dtid);

void tx_delete(uint32_t id);

void tx_gc_start();

#endif
