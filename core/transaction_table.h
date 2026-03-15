#ifndef TX_TABLE_H
#define TX_TABLE_H

#include <stdint.h>

void tx_table_init();

void tx_store(uint32_t otid, int backend);

int tx_lookup(uint32_t dtid);

void tx_delete(uint32_t id);

void tx_gc_start();

#endif
