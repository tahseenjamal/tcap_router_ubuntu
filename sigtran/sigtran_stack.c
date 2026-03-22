#include "sigtran_stack.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "../core/msg_pool.h"
#include "../core/worker_pool.h"
#include "../router/tcap_parser.h"
#include "m3ua.h"

#define DIR_FROM_STP 0

extern int m3ua_client_connect(const char *ip, int port);
extern int m3ua_recv(uint8_t *out, int max_len);

/* ===================================== */

void *stp_reader(void *arg) {
  (void)arg;

  uint8_t buf[4096];

  while (1) {
    int r = m3ua_recv(buf, sizeof(buf));
    if (r <= 0)
      continue;

    int sccp_len = 0;
    m3ua_hdr_t *h = (m3ua_hdr_t *)buf;

    if (h->msg_class != M3UA_CLASS_TRANSFER) {
      continue;
    }
    uint8_t *sccp = buf + sizeof(m3ua_hdr_t);
    sccp_len = r - sizeof(m3ua_hdr_t);

    struct msgb *msg = msg_pool_get();
    memcpy(msg->data, sccp, sccp_len);
    msg->len = sccp_len;

    uint32_t otid = 0, dtid = 0;
    int type = 0;

    parse_tcap(msg->data, msg->len, &otid, &dtid, &type);

    msg->cb[0] = DIR_FROM_STP;

    worker_enqueue(msg, otid, dtid, type);
  }

  return NULL;
}

/* ===================================== */

void sigtran_start() {
  printf("Connecting to STP via M3UA\n");

  m3ua_client_connect("127.0.0.1", 2905);

  pthread_t t;
  pthread_create(&t, NULL, stp_reader, NULL);
  pthread_detach(t);
}
