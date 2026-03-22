#include "m3ua.h"

#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* ===================================== */
/* GLOBAL SOCKET */
/* ===================================== */

int m3ua_sock = -1;

/* ===================================== */
/* CONNECT */
/* ===================================== */

int m3ua_client_connect(const char *ip, int port) {

  m3ua_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
  if (m3ua_sock < 0) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
    perror("inet_pton");
    return -1;
  }

  if (connect(m3ua_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("connect");
    return -1;
  }

  printf("Connected to STP %s:%d\n", ip, port);
  return 0;
}

/* ===================================== */
/* BUILD FULL M3UA DATA (CORRECT RFC4666) */
/* ===================================== */

static int build_m3ua_data(uint8_t *out, uint8_t *sccp, int sccp_len) {

  uint8_t *p = out;

  m3ua_hdr_t *h = (m3ua_hdr_t *)p;
  p += sizeof(m3ua_hdr_t);

  /* -------------------------------
   * Routing Context TLV
   * ------------------------------- */
  *(uint16_t *)p = htons(0x0006);
  *(uint16_t *)(p + 2) = htons(8);
  *(uint32_t *)(p + 4) = htonl(1);
  p += 8;

  /* -------------------------------
   * Protocol Data TLV
   * ------------------------------- */
  *(uint16_t *)p = htons(0x0210);

  int pd_len = 12 + sccp_len;
  *(uint16_t *)(p + 2) = htons(pd_len + 4);

  uint8_t *pd = p + 4;

  /* MTP3 Header */
  *(uint32_t *)(pd + 0) = htonl(1); // OPC
  *(uint32_t *)(pd + 4) = htonl(2); // DPC

  pd[8] = 3;  // SI = SCCP
  pd[9] = 2;  // NI
  pd[10] = 0; // MP
  pd[11] = 0; // SLS

  memcpy(pd + 12, sccp, sccp_len);

  p += 4 + pd_len;

  /* -------------------------------
   * M3UA Header
   * ------------------------------- */
  h->version = 1;
  h->reserved = 0;
  h->msg_class = M3UA_CLASS_TRANSFER;
  h->msg_type = M3UA_DATA;
  h->length = htonl(p - out);

  return p - out;
}

/* ===================================== */
/* SEND (AUTO WRAPS SCCP → M3UA) */
/* ===================================== */

int m3ua_send(uint8_t *sccp, int sccp_len) {

  if (m3ua_sock <= 0) {
    printf("M3UA socket not ready\n");
    return -1;
  }

  uint8_t buf[2048];

  int len = build_m3ua_data(buf, sccp, sccp_len);

  struct msghdr msg = {0};
  struct iovec iov;

  char cmsgbuf[CMSG_SPACE(sizeof(struct sctp_sndrcvinfo))];
  memset(cmsgbuf, 0, sizeof(cmsgbuf));

  iov.iov_base = buf;
  iov.iov_len = len;

  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if (!cmsg) {
    printf("CMSG_FIRSTHDR failed\n");
    return -1;
  }

  cmsg->cmsg_level = IPPROTO_SCTP;
  cmsg->cmsg_type = SCTP_SNDRCV;
  cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));

  struct sctp_sndrcvinfo *sinfo =
      (struct sctp_sndrcvinfo *)CMSG_DATA(cmsg);

  memset(sinfo, 0, sizeof(*sinfo));
  sinfo->sinfo_stream = 0;
  sinfo->sinfo_ppid = htonl(3); // 🔥 CRITICAL

  msg.msg_controllen = cmsg->cmsg_len;

  int ret = sendmsg(m3ua_sock, &msg, 0);
  if (ret < 0) {
    perror("sendmsg");
  }

  return ret;
}

/* ===================================== */
/* RECEIVE */
/* ===================================== */

int m3ua_recv(uint8_t *out, int max_len) {

  if (m3ua_sock <= 0) {
    printf("M3UA socket not ready (recv)\n");
    return -1;
  }

  return recv(m3ua_sock, out, max_len, 0);
}
