#include <osmocom/core/select.h>
#include <stdio.h>

#include "core/backend_pool.h"
#include "core/transaction_table.h"
#include "core/worker_pool.h"
#include "network/sctp_server.h"
#include "sigtran/sigtran_stack.h"

int main() {
    printf("Starting TCAP Router\n");

    backend_pool_init();

    tx_table_init();
    tx_gc_start();

    worker_pool_init();

    sigtran_start();

    /* Start SCTP server for osmo-stp */
    sctp_server_start(2905);

    while (1) osmo_select_main(0);

    return 0;
}
