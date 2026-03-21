#include <osmocom/core/select.h>
#include <stdio.h>

#include "core/backend_server.h"
#include "core/msg_pool.h"
#include "core/transaction_table.h"
#include "core/worker_pool.h"
#include "sigtran/sigtran_stack.h"

int main() {
  printf("Starting TCAP Router\n");

  msg_pool_init();

  tx_table_init();
  tx_gc_start();

  worker_pool_init();

  /* SIGTRAN → connects to osmo-stp */
  sigtran_start();

  /* Backend apps connect here */
  backend_server_start(2906);

  while (1)
    osmo_select_main(0);

  return 0;
}
