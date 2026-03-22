#include "m3ua.h"

#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../core/backend_server.h"
#include "../core/msg_pool.h"
#include "../core/worker_pool.h"
#include "../router/tcap_parser.h"

#define MAX_EVENTS 1024
#define MAX_PACKET 4096

#define DIR_FROM_BACKEND 1

/* ===================================== */
/* SAFE M3UA PARSER */
/* ===================================== */

static uint8_t *extract_m3ua_payload(uint8_t *buf, int len, int *out_len) {
  if (!buf || len < (int)sizeof(m3ua_hdr_t))
    return NULL;

  m3ua_hdr_t *h = (m3ua_hdr_t *)buf;

  /* version check */
  if (h->version != M3UA_VERSION)
    return NULL;

  /* only DATA supported */
  if (h->msg_class != M3UA_CLASS_TRANSFER || h->msg_type != M3UA_DATA)
    return NULL;

  /* length validation */
  int mlen = ntohl(h->length);

  if (mlen <= (int)sizeof(m3ua_hdr_t) || mlen > len)
    return NULL;

  int payload_len = mlen - sizeof(m3ua_hdr_t);

  if (payload_len <= 0)
    return NULL;

  *out_len = payload_len;
  return buf + sizeof(m3ua_hdr_t);
}

/* ===================================== */
/* SERVER */
/* ===================================== */

void m3ua_server_start(int port) {
  int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
  if (listen_sock < 0) {
    perror("socket");
    return;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(listen_sock);
    return;
  }

  if (listen(listen_sock, 128) < 0) {
    perror("listen");
    close(listen_sock);
    return;
  }

  int ep = epoll_create1(0);
  if (ep < 0) {
    perror("epoll_create");
    close(listen_sock);
    return;
  }

  struct epoll_event ev = {0};
  ev.events = EPOLLIN;
  ev.data.fd = listen_sock;

  if (epoll_ctl(ep, EPOLL_CTL_ADD, listen_sock, &ev) < 0) {
    perror("epoll_ctl listen");
    close(listen_sock);
    return;
  }

  printf("M3UA server listening on %d\n", port);

  while (1) {
    struct epoll_event events[MAX_EVENTS];

    int n = epoll_wait(ep, events, MAX_EVENTS, -1);
    if (n < 0) {
      perror("epoll_wait");
      continue;
    }

    for (int i = 0; i < n; i++) {

      /* =====================================
       * NEW CONNECTION
       * ===================================== */
      if (events[i].data.fd == listen_sock) {

        int client = accept(listen_sock, NULL, NULL);
        if (client < 0) {
          perror("accept");
          continue;
        }

        ev.events = EPOLLIN;
        ev.data.fd = client;

        if (epoll_ctl(ep, EPOLL_CTL_ADD, client, &ev) < 0) {
          perror("epoll_ctl client");
          close(client);
          continue;
        }

        printf("Backend connected fd=%d\n", client);
        continue;
      }

      /* =====================================
       * DATA FROM BACKEND
       * ===================================== */

      int fd = events[i].data.fd;

      uint8_t buf[MAX_PACKET];

      int r = recv(fd, buf, sizeof(buf), 0);

      if (r <= 0) {
        printf("Backend disconnected fd=%d\n", fd);
        epoll_ctl(ep, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        continue;
      }

      /* =====================================
       * M3UA PARSE
       * ===================================== */

      int sccp_len = 0;
      uint8_t *sccp = extract_m3ua_payload(buf, r, &sccp_len);

      if (!sccp) {
        printf("DROP: invalid M3UA from fd=%d len=%d\n", fd, r);
        continue;
      }

      if (sccp_len <= 0 || sccp_len > MAX_PACKET) {
        printf("DROP: invalid SCCP length=%d\n", sccp_len);
        continue;
      }

      /* =====================================
       * MSG ALLOC
       * ===================================== */

      msg_t *msg = msg_pool_get();
      if (!msg) {
        printf("DROP: msg pool exhausted\n");
        continue;
      }

      memcpy(msg->data, sccp, sccp_len);
      msg->len = sccp_len;

      /* =====================================
       * TCAP PARSE
       * ===================================== */

      uint32_t otid = 0, dtid = 0;
      int type = 0;

      parse_tcap(msg->data, msg->len, &otid, &dtid, &type);

      /* =====================================
       * ROUTING METADATA
       * ===================================== */

      msg->cb[0] = DIR_FROM_BACKEND;
      memcpy(&msg->cb[1], &fd, sizeof(int));

      /* =====================================
       * DISPATCH
       * ===================================== */

      worker_enqueue(msg, otid, dtid, type);
    }
  }
}

/* ===================================== */
/* THREAD WRAPPER */
/* ===================================== */

void *m3ua_server_thread(void *arg) {
  int port = *(int *)arg;
  m3ua_server_start(port);
  return NULL;
}
