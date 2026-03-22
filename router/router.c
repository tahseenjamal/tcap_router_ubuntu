#include "router.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../core/msg_pool.h"
#include "../core/transaction_table.h"
#include "sccp_gt.h"

#define DIR_FROM_STP 0
#define DIR_FROM_BACKEND 1

/* M3UA send (to STP) */
extern int m3ua_send(uint8_t *data, int len);

/* backend registry */
extern int backend_fds[];
extern atomic_int backend_count;
extern atomic_int rr;

/* cleanup */
extern void remove_backend(int fd);

/* ============================================
 * Self GT
 * ============================================ */
static uint8_t SELF_GT[] = {0x91, 0x88, 0x77, 0x66};
static int SELF_GT_LEN = 4;

/* ============================================
 * Safe send (to backend)
 * ============================================ */
static int send_full(int fd, uint8_t *buf, int len) {
  int sent = 0;

  while (sent < len) {
    int s = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
    if (s <= 0)
      return -1;
    sent += s;
  }

  return 0;
}

/* ============================================
 * Backend selection
 * ============================================ */
static int choose_backend_fd() {
  int n = atomic_load(&backend_count);

  if (n <= 0)
    return -1;

  int start = atomic_fetch_add(&rr, 1);

  for (int k = 0; k < n; k++) {
    int fd = backend_fds[(start + k) % n];
    if (fd > 0)
      return fd;
  }

  return -1;
}

/* ============================================
 * Extract backend fd
 * ============================================ */
static inline int get_backend_fd(struct msgb *msg) {
  int fd = -1;
  memcpy(&fd, &msg->cb[1], sizeof(int));
  return fd;
}

/* ============================================
 * Extract SCCP payload
 * ============================================ */
static uint8_t *extract_sccp_userdata(uint8_t *d, int len, int *out_len) {
  if (len < 5)
    return NULL;

  uint8_t msg_type = d[0];

  if (msg_type != 0x09 && msg_type != 0x11 && msg_type != 0x13)
    return NULL;

  uint8_t ptr_data = d[3];

  if (ptr_data == 0 || ptr_data >= len)
    return NULL;

  int offset = ptr_data + 1;

  if (offset >= len)
    return NULL;

  *out_len = len - offset;
  return &d[offset];
}

/* ============================================
 * Send to STP (M3UA)
 * ============================================ */
static void send_to_stp(struct msgb *msg) {
  if (m3ua_send(msg->data, msg->len) < 0) {
    printf("STP send failed\n");
  }
  msg_pool_put(msg);
}

/* ============================================
 * Send to backend
 * ============================================ */
static void send_to_backend_fd(int fd, struct msgb *msg) {
  if (send_full(fd, msg->data, msg->len) < 0) {
    printf("Backend send failed fd=%d\n", fd);
    remove_backend(fd);
    close(fd);
  }

  msg_pool_put(msg);
}

/* ============================================
 * MAIN ROUTER
 * ============================================ */
void route_tcap(struct msgb *msg, uint32_t otid, uint32_t dtid, int type) {

  int direction = msg->cb[0];

  /* ============================================
   * BACKEND → STP
   * ============================================ */
  if (direction == DIR_FROM_BACKEND) {

    int backend_fd = get_backend_fd(msg);

    if (msg->len <= 0) {
      msg_pool_put(msg);
      return;
    }

    int tcap_len = 0;
    uint8_t *tcap = extract_sccp_userdata(msg->data, msg->len, &tcap_len);

    if (!tcap) {
      printf("DROP: invalid SCCP payload (backend)\n");
      msg_pool_put(msg);
      return;
    }

    if (!dtid) {
      printf("DROP: missing dtid (BACKEND→STP)\n");
      msg_pool_put(msg);
      return;
    }

    tx_info_t info;

    if (tx_lookup_full(dtid, &info) < 0) {
      printf("DROP: tx_lookup failed dtid=%u\n", dtid);
      msg_pool_put(msg);
      return;
    }

    if (info.gt_len > 0)
      rewrite_calling_gt(msg->data, msg->len, info.gt, info.gt_len);

    if (type == 3 || type == 2)
      tx_delete(dtid);

    send_to_stp(msg);
    return;
  }

  /* ============================================
   * STP → BACKEND
   * ============================================ */

  int tcap_len = 0;
  uint8_t *tcap = extract_sccp_userdata(msg->data, msg->len, &tcap_len);

  if (!tcap) {
    printf("DROP: invalid SCCP payload (stp)\n");
    msg_pool_put(msg);
    return;
  }

  int is_begin = (type == 1 && otid != 0);
  int backend_fd = -1;

  if (is_begin) {
    backend_fd = choose_backend_fd();

    if (backend_fd < 0) {
      printf("DROP: no backend available\n");
      msg_pool_put(msg);
      return;
    }

    uint8_t orig_gt[32];
    int gt_len = 0;

    if (extract_calling_gt(msg->data, msg->len, orig_gt, &gt_len) < 0)
      gt_len = 0;

    rewrite_calling_gt(msg->data, msg->len, SELF_GT, SELF_GT_LEN);

    tx_store_full(otid, backend_fd, orig_gt, gt_len);

  } else {

    if (!dtid) {
      printf("DROP: missing dtid (STP→BACKEND)\n");
      msg_pool_put(msg);
      return;
    }

    tx_info_t info;

    if (tx_lookup_full(dtid, &info) < 0) {
      printf("DROP: tx_lookup failed dtid=%u\n", dtid);
      msg_pool_put(msg);
      return;
    }

    backend_fd = info.backend;

    if (info.gt_len > 0)
      rewrite_calling_gt(msg->data, msg->len, info.gt, info.gt_len);

    if (type == 3 || type == 2)
      tx_delete(dtid);
  }

  send_to_backend_fd(backend_fd, msg);
}
