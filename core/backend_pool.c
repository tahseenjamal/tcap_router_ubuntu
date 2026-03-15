#include "backend_pool.h"

#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../config.h"

static backend_t backends[MAX_BACKENDS];

static atomic_int rr;

static int backend_total = 2;

static const char *backend_ip[MAX_BACKENDS] = {"127.0.0.1", "127.0.0.1"};

static int backend_port[MAX_BACKENDS] = {4000, 4001};

/* ------------------------------------------------ */

static int connect_backend(const char *ip, int port) {
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);

  if (sock < 0)
    return -1;

  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  inet_pton(AF_INET, ip, &addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {

    close(sock);

    return -1;
  }

  return sock;
}

/* ------------------------------------------------ */

static void *backend_health_thread(void *arg) {
  (void)arg;

  while (1) {

    sleep(5);

    for (int i = 0; i < backend_total; i++) {

      if (atomic_load(&backends[i].active))
        continue;

      printf("Attempting reconnect backend %d\n", i);

      int sock = connect_backend(backend_ip[i], backend_port[i]);

      if (sock < 0)
        continue;

      backends[i].sock = sock;

      atomic_store(&backends[i].active, 1);

      printf("Backend %d reconnected\n", i);
    }
  }

  return NULL;
}

/* ------------------------------------------------ */

void backend_pool_init() {
  atomic_store(&rr, 0);

  for (int i = 0; i < backend_total; i++) {

    backends[i].id = i;

    atomic_store(&backends[i].load, 0);

    int sock = connect_backend(backend_ip[i], backend_port[i]);

    if (sock < 0) {

      atomic_store(&backends[i].active, 0);

      printf("Backend %d connection failed\n", i);

      continue;
    }

    backends[i].sock = sock;

    atomic_store(&backends[i].active, 1);

    printf("Backend %d connected\n", i);
  }

  pthread_t t;

  pthread_create(&t, NULL, backend_health_thread, NULL);

  pthread_detach(t);
}

/* ------------------------------------------------ */

backend_t *backend_choose() {
  int start = atomic_fetch_add(&rr, 1);

  for (int i = 0; i < backend_total; i++) {

    int idx = (start + i) % backend_total;

    if (atomic_load(&backends[idx].active))
      return &backends[idx];
  }

  return NULL;
}

/* ------------------------------------------------ */

backend_t *backend_get(int id) {
  if (id < 0 || id >= backend_total)
    return NULL;

  if (!atomic_load(&backends[id].active))
    return NULL;

  return &backends[id];
}
