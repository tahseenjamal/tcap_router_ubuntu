#include <osmocom/core/select.h>
#include <stdio.h>

#include "core/backend_pool.h"
#include "core/backend_server.h"  // NEW
#include "core/transaction_table.h"
#include "core/worker_pool.h"
#include "sigtran/sigtran_stack.h"

int main() {
    printf("Starting TCAP Router\n");

    backend_pool_init();  // temporary (can remove later)

    tx_table_init();
    tx_gc_start();

    worker_pool_init();

    sigtran_start();

    /* NEW: backend server */
    backend_server_start(2906);

    while (1) osmo_select_main(0);

    return 0;
}
