#include "m3ua.h"

#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../core/msg_pool.h"
#include "../core/worker_pool.h"
#include "../router/tcap_parser.h"

#define MAX_EVENTS 1024
#define MAX_PACKET 4096

#define DIR_FROM_BACKEND 1

/* ===================================== */
/* M3UA → SCCP EXTRACTOR (RFC4666 style) */
/* ===================================== */

static uint8_t *extract_sccp_from_m3ua(uint8_t *buf, int len, int *out_len) {
  if (!buf || len < (int)sizeof(m3ua_hdr_t))
    return NULL;

  m3ua_hdr_t *h = (m3ua_hdr_t *)buf;

  /* basic validation */
  if (h->version != M3UA_VERSION)
    return NULL;

  if (h->msg_class != M3UA_CLASS_TRANSFER ||
      h->msg_type != M3UA_DATA)
    return NULL;

  int mlen = ntohl(h->length);

  if (mlen > len || mlen < (int)sizeof(m3ua_hdr_t))
    return NULL;

  /* start of TLVs */
  uint8_t *p = buf + sizeof(m3ua_hdr_t);
  int remain = mlen - sizeof(m3ua_hdr_t);

  while (remain > 4) {

    uint16_t tag = ntohs(*(uint16_t *)p);
    uint16_t plen = ntohs(*(uint16_t *)(p + 2));

    if (plen < 4 || plen > remain)
      return NULL;

    /* =====================================
     * PROTOCOL DATA (0x0210)
     * ===================================== */
    if (tag == 0x0210) {

      uint8_t *pd = p + 4;

      if (plen < 16) /* 4 (TLV hdr) + 12 (MTP3) */
        return NULL;

      /* skip MTP3 header (12 bytes) */
      uint8_t *sccp = pd + 12;

      int sccp_len = plen - 4 - 12;

      if (sccp_len <= 0)
        return NULL;

      *out_len = sccp_len;
      return sccp;
    }

    /* move to next TLV (4-byte aligned) */
    int aligned = (plen + 3) & ~3;
    p += aligned;
    remain -= aligned;
  }

  return NULL;
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
       * M3UA PARSE (FIXED)
       * ===================================== */

      int sccp_len = 0;
      uint8_t *sccp = extract_sccp_from_m3ua(buf, r, &sccp_len);

      if (!sccp) {
        printf("DROP: invalid M3UA (no SCCP) fd=%d len=%d\n", fd, r);
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
