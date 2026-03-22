#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "core/msg_pool.h"
#include "core/transaction_table.h"
#include "core/worker_pool.h"
#include "sigtran/m3ua_server.h"
#include "sigtran/sigtran_stack.h"

int main() {
  printf("Starting TCAP Router (M3UA enabled)\n");

  msg_pool_init();

  tx_table_init();
  tx_gc_start();

  worker_pool_init();

  sigtran_start();

  /* Run M3UA server in thread */
  pthread_t t;
  int port = 2906;
  pthread_create(&t, NULL, m3ua_server_thread, &port);
  pthread_detach(t);


  while (1) {
    sleep(1);
  }

  return 0;
}
